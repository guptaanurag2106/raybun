#include <cJSON.h>
#include <microhttpd.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "api.h"
#include "libmicrohttpd-1.0.1/src/include/microhttpd.h"
#include "state.h"
#include "utils.h"

static void free_conn_info(void *cls, struct MHD_Connection *connection,
                           void **con_cls,
                           enum MHD_RequestTerminationCode toe) {
    UNUSED(cls);
    UNUSED(connection);
    UNUSED(toe);
    ConnectionInfo *ci = *con_cls;
    if (!ci) return;

    if (ci->buffer) free(ci->buffer);

    if (ci->tmpfile) fclose(ci->tmpfile);

    free(ci);
    *con_cls = NULL;
}

static enum MHD_Result answer_get_request(
    void *cls, struct MHD_Connection *connection, const char *url,
    const char *method, const char *version, const char *upload_data,
    unsigned long *upload_data_size, void **con_cls) {
    MasterAPIContext *context = cls;
    UNUSED(version);
    UNUSED(upload_data);
    UNUSED(upload_data_size);
    UNUSED(con_cls);

    if (strcmp(method, "GET") == 0) {
        if (strcmp(url, "/api/scene") == 0) {
            Log(Log_Info, "Master: Serving scene to worker");
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "scene_crc",
                                    context->scene->scene_crc);
            cJSON_AddStringToObject(root, "scene_json",
                                    context->scene->scene_json);
            char *json = cJSON_PrintUnformatted(root);

            struct MHD_Response *resp = MHD_create_response_from_buffer(
                strlen(json), json, MHD_RESPMEM_MUST_FREE);
            cJSON_Delete(root);
            MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_TYPE,
                                    "application/json");
            int ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
            MHD_destroy_response(resp);

            return ret;
        }
        if (strcmp(url, "/api/work") == 0) {
            const char *worker_id = MHD_lookup_connection_value(
                connection, MHD_GET_ARGUMENT_KIND, "worker_id");
            const char *scene_crc_c = MHD_lookup_connection_value(
                connection, MHD_GET_ARGUMENT_KIND, "scene_crc");

            if (!worker_id || !scene_crc_c) {
                const char *notfound =
                    "{\"error\":\"/api/work missing parameters\"}";
                struct MHD_Response *resp = MHD_create_response_from_buffer(
                    strlen(notfound), (void *)notfound, MHD_RESPMEM_PERSISTENT);
                MHD_add_response_header(resp, "Content-Type",
                                        "application/json");
                int ret =
                    MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, resp);
                MHD_destroy_response(resp);
                return ret;
            }

            unsigned int scene_crc = atoi(scene_crc_c);
            if (context->scene->scene_crc != scene_crc) {
                const char *notfound =
                    "{\"error\":\"/api/work wrong scene crc found\"}";
                struct MHD_Response *resp = MHD_create_response_from_buffer(
                    strlen(notfound), (void *)notfound, MHD_RESPMEM_PERSISTENT);
                MHD_add_response_header(resp, "Content-Type",
                                        "application/json");
                int ret =
                    MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, resp);
                MHD_destroy_response(resp);
                return ret;
            }

            int curr_tile = atomic_fetch_add(&context->work->tile_finished, 1);
            if (curr_tile >= context->work->tile_count) {
                const char *done =
                    "{\"status\":\"/api/work all work done, no tiles left\"}";
                struct MHD_Response *resp = MHD_create_response_from_buffer(
                    strlen(done), (void *)done, MHD_RESPMEM_PERSISTENT);
                MHD_add_response_header(resp, "Content-Type",
                                        "application/json");
                int ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
                MHD_destroy_response(resp);
                return ret;
            }

            Tile tile = context->work->tiles[curr_tile];
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "tile_id", curr_tile);
            cJSON *tilej = cJSON_CreateObject();
            cJSON_AddNumberToObject(tilej, "x", tile.x);
            cJSON_AddNumberToObject(tilej, "y", tile.y);
            cJSON_AddNumberToObject(tilej, "tw", tile.tw);
            cJSON_AddNumberToObject(tilej, "th", tile.th);
            cJSON_AddItemToObject(root, "tile", tilej);
            char *json = cJSON_PrintUnformatted(root);

            struct MHD_Response *resp = MHD_create_response_from_buffer(
                strlen(json), json, MHD_RESPMEM_MUST_FREE);
            cJSON_Delete(root);
            MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_TYPE,
                                    "application/json");
            int ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
            MHD_destroy_response(resp);

            return ret;

        } else {
            const char *notfound = "{\"error\":\"not found\"}";
            struct MHD_Response *resp = MHD_create_response_from_buffer(
                strlen(notfound), (void *)notfound, MHD_RESPMEM_PERSISTENT);
            MHD_add_response_header(resp, "Content-Type", "application/json");
            int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, resp);
            MHD_destroy_response(resp);
            return ret;
        }
    }
    if (strcmp(method, "POST") == 0) {
        return MHD_NO;
    } else {
        const char *mna = "{\"error\":\"method not allowed\"}";
        struct MHD_Response *resp = MHD_create_response_from_buffer(
            strlen(mna), (void *)mna, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(resp, "Content-Type", "application/json");
        int ret =
            MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, resp);
        MHD_destroy_response(resp);
        return ret;
    }
}

bool master_start_server(int port, MasterAPIContext *context) {
    Log(Log_Info, "Master: Starting server on port %d", port);

#ifdef DEBUG
    master_daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION |
            MHD_USE_DEBUG,
        port, NULL, NULL, &answer_get_request, context,
        MHD_OPTION_NOTIFY_COMPLETED, free_conn_info, NULL, MHD_OPTION_END);
#else
    master_daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION, port,
        NULL, NULL, &answer_get_request, context, MHD_OPTION_NOTIFY_COMPLETED,
        free_conn_info, NULL, MHD_OPTION_END);
#endif

    if (master_daemon == NULL) {
        free(context);
        return false;
    }

    return true;
}
