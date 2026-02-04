#define _POSIX_C_SOURCE 200809L
#include <microhttpd.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "cJSON.h"
#include "libmicrohttpd-1.0.1/src/include/microhttpd.h"
#include "utils.h"

static int send_response(struct MHD_Connection *connection, unsigned int status,
                         const char *msg) {
    struct MHD_Response *resp = MHD_create_response_from_buffer(
        strlen(msg), (void *)msg, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_TYPE,
                            "application/json");
    int ret = MHD_queue_response(connection, status, resp);
    MHD_destroy_response(resp);
    return ret;
}

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
    UNUSED(version);
    MasterAPIContext *context = cls;
    ConnectionInfo *ci = *con_cls;

    if (strcmp(method, "GET") == 0) {
        if (strcmp(url, "/api/scene") == 0) {
            Log(Log_Info, "Master: Serving scene %u to worker",
                context->scene->scene_crc);
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "scene_crc",
                                    context->scene->scene_crc);
            cJSON_AddStringToObject(root, "scene_json",
                                    context->scene->scene_json);
            char *json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);

            int ret = send_response(connection, MHD_HTTP_OK, json);
            free(json);
            return ret;
        }

        if (strcmp(url, "/api/work") == 0) {
            const char *worker_id = MHD_lookup_connection_value(
                connection, MHD_GET_ARGUMENT_KIND, "worker_id");
            const char *scene_crc_c = MHD_lookup_connection_value(
                connection, MHD_GET_ARGUMENT_KIND, "scene_crc");

            if (!worker_id || !scene_crc_c) {
                Log(Log_Warn, "Master: /api/work request missing parameters");
                const char *badrequest =
                    "{\"error\":\"/api/work missing parameters\"}";
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                                     badrequest);
            }

            unsigned int scene_crc = atoi(scene_crc_c);
            if (context->scene->scene_crc != scene_crc) {
                Log(Log_Warn,
                    "Master: Worker '%s' sent wrong scene CRC: %u (expected "
                    "%u)",
                    worker_id, scene_crc, context->scene->scene_crc);
                const char *notfound =
                    "{\"error\":\"/api/work wrong scene crc found\"}";
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                                     notfound);
            }

            // TODO: dont add until done? have separate variable/array of done,
            // in-flight
            // TODO: check perf if we want to give anything to this worker
            int curr_tile = atomic_fetch_add(&context->work->tile_finished, 1);
            if (curr_tile >= context->work->tile_count) {
                Log(Log_Info,
                    "Master: Worker '%s' requested work but no tiles left",
                    worker_id);
                const char *done =
                    "{\"status\":\"/api/work all work done, no tiles left\"}";
                return send_response(connection, MHD_HTTP_OK, done);
            }

            Tile tile = context->work->tiles[curr_tile];
            Log(Log_Debug, "Master: Assigned tile %d (%d,%d %dx%d) to '%s'",
                curr_tile, tile.x, tile.y, tile.tw, tile.th, worker_id);

            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "tile_id", curr_tile);
            cJSON *tilej = cJSON_CreateObject();
            cJSON_AddNumberToObject(tilej, "x", tile.x);
            cJSON_AddNumberToObject(tilej, "y", tile.y);
            cJSON_AddNumberToObject(tilej, "tw", tile.tw);
            cJSON_AddNumberToObject(tilej, "th", tile.th);
            cJSON_AddItemToObject(root, "tile", tilej);
            char *json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);

            int ret = send_response(connection, MHD_HTTP_OK, json);
            free(json);
            return ret;
        }

        Log(Log_Debug, "Master: 404 Not Found: %s %s", method, url);
        const char *notfound = "{\"error\":\"not found\"}";
        return send_response(connection, MHD_HTTP_NOT_FOUND, notfound);
    }

    if (strcmp(method, "POST") == 0) {
        if (ci == NULL) {
            ci = calloc(1, sizeof(*ci));
            if (!ci) {
                Log(Log_Error, "Master: Failed to allocate ConnectionInfo");
                return MHD_NO;
            }

            const char *cl = MHD_lookup_connection_value(
                connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
            ci->content_length = cl ? strtoull(cl, NULL, 10) : 0;

            if (ci->content_length > MAX_PAYLOAD) {
                Log(Log_Warn,
                    "Master: Rejecting POST to %s - Payload too large (%zu "
                    "bytes)",
                    url, ci->content_length);
                return send_response(connection, MHD_HTTP_CONTENT_TOO_LARGE,
                                     "Payload too large\n");
            }
            *con_cls = ci;
            return MHD_YES;
        }
        if (*upload_data_size != 0) {
            size_t chunk = *upload_data_size;

            if (ci->received + chunk > MAX_PAYLOAD) {
                Log(Log_Warn,
                    "Master: POST payload exceeded limit during upload");
                return send_response(connection, MHD_HTTP_CONTENT_TOO_LARGE,
                                     "Payload too large\n");
            }

            if (!ci->use_tempfile && ci->received + chunk <= SMALL_THRESHOLD) {
                if (!ci->buffer) {
                    ci->buffer_size = SMALL_THRESHOLD;
                    ci->buffer = malloc(ci->buffer_size);
                }
                memcpy(ci->buffer + ci->received, upload_data, chunk);
            } else {
                if (!ci->use_tempfile) {
                    ci->use_tempfile = 1;
                    ci->tmpfile = tmpfile();

                    if (!ci->tmpfile) {
                        Log(Log_Error,
                            "Master: Cannot create temp file for upload");
                        return send_response(connection,
                                             MHD_HTTP_INTERNAL_SERVER_ERROR,
                                             "Cannot process "
                                             "request\n");
                    }

                    if (ci->buffer && ci->received)
                        fwrite(ci->buffer, 1, ci->received, ci->tmpfile);

                    free(ci->buffer);
                    ci->buffer = NULL;
                }

                fwrite(upload_data, 1, chunk, ci->tmpfile);
            }

            ci->received += chunk;
            *upload_data_size = 0;
            return MHD_YES;
        }

        cJSON *root = NULL;
        if (!ci->processed) {
            ci->processed = 1;
            char *json_data = NULL;

            if (!ci->use_tempfile) {
                json_data = malloc(ci->received + 1);
                memcpy(json_data, ci->buffer, ci->received);
                json_data[ci->received] = '\0';
            } else {
                json_data = malloc(ci->received + 1);
                fseek(ci->tmpfile, 0, SEEK_SET);
                fread(json_data, 1, ci->received, ci->tmpfile);
                json_data[ci->received] = '\0';
            }
            root = cJSON_Parse(json_data);
            free(json_data);
        }

        if (strcmp(url, "/api/register") == 0) {
            if (!root) {
                Log(Log_Warn, "Master: /api/register received invalid JSON");
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                                     "{\"error\":\"Invalid JSON\"}");
            }

            MachineInfo info = {0};
            cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
            cJSON *perf = cJSON_GetObjectItemCaseSensitive(root, "perf");
            cJSON *thread_count =
                cJSON_GetObjectItemCaseSensitive(root, "thread_count");
            cJSON *simd = cJSON_GetObjectItemCaseSensitive(root, "simd");

            if (!cJSON_IsNumber(perf) || !cJSON_IsNumber(thread_count) ||
                !cJSON_IsNumber(simd) || !cJSON_IsString(name)) {
                Log(Log_Warn, "Master: /api/register received invalid JSON");
                cJSON_Delete(root);
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                                     "{\"error\":\"Invalid JSON parameters\"}");
            }

            // TODO: check unique names
            info.name = strdup(name->valuestring);
            info.perf = perf->valuedouble;
            info.thread_count = thread_count->valueint;
            if (info.thread_count <= 0 || info.perf < 0 || info.perf > 10) {
                Log(Log_Warn, "Master: /api/register received invalid JSON");
                cJSON_Delete(root);
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                                     "{\"error\":\"Invalid JSON parameters\"}");
            }
            info.simd = simd->valueint;

            Log(Log_Info, "Master: Registering worker '%s' (Perf: %.2f)",
                info.name, info.perf);

            vec_push(&context->workers, info);

            cJSON_Delete(root);
            return send_response(connection, MHD_HTTP_OK, "{\"success\":true}");
        }

        if (strcmp(url, "/api/result") == 0) {
            if (!root) {
                Log(Log_Warn, "Master: /api/result received invalid JSON");
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                                     "{\"error\":\"Invalid JSON\"}");
            }

            cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
            cJSON *tile_id = cJSON_GetObjectItemCaseSensitive(root, "tile_id");
            cJSON *pixels = cJSON_GetObjectItemCaseSensitive(root, "pixels");

            if (!cJSON_IsNumber(tile_id) || !cJSON_IsString(pixels) ||
                !cJSON_IsString(name)) {
                cJSON_Delete(root);
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                                     "{\"error\":\"Invalid JSON parameters\"}");
            }

            // TODO: check if worker registered, and for that tile
            const char *worker_name = name->valuestring;

            int tid = tile_id->valueint;
            // TODO: check if tile already done
            if (tid < 0 || tid >= context->work->tile_count) {
                cJSON_Delete(root);
                Log(Log_Warn, "Master: Worker sent invalid tile_id %d", tid);
                return send_response(connection, MHD_HTTP_BAD_REQUEST,
                                     "{\"error\":\"Invalid tile_id\"}");
            }

            Tile tile = context->work->tiles[tid];
            const char *hex_pixels = pixels->valuestring;
            int expected_len =
                tile.tw * tile.th * 8;  // 8 hex chars per pixel (ARGB)

            if ((int)strlen(hex_pixels) != expected_len) {
                cJSON_Delete(root);
                Log(Log_Warn,
                    "Master: Pixel data length mismatch. Expected %d, got %zu",
                    expected_len, strlen(hex_pixels));
                return send_response(
                    connection, MHD_HTTP_BAD_REQUEST,
                    "{\"error\":\"Pixel data length mismatch\"}");
            }

            // Parse hex string and write to image buffer
            for (int y = 0; y < tile.th; y++) {
                for (int x = 0; x < tile.tw; x++) {
                    char hex[9];
                    // FIX:better method
                    memcpy(hex, hex_pixels, 8);
                    hex[8] = '\0';
                    uint32_t pixel = (uint32_t)strtoul(hex, NULL, 16);

                    int image_idx =
                        (tile.y + y) * context->state->width + (tile.x + x);
                    context->state->image[image_idx] = pixel;

                    hex_pixels += 8;
                }
            }

            // TODO:mark the tile done (separate array?)

            Log(Log_Debug, "Master: Received result for tile %d from '%s'", tid,
                worker_name);

            cJSON_Delete(root);
            return send_response(connection, MHD_HTTP_OK, "{\"success\":true}");
        }

        Log(Log_Debug, "Master: 404 Not Found (POST): %s", url);
        const char *notfound = "{\"error\":\"not found\"}";
        return send_response(connection, MHD_HTTP_NOT_FOUND, notfound);
    }

    Log(Log_Warn, "Master: Method Not Allowed: %s %s", method, url);
    const char *mna = "{\"error\":\"method not allowed\"}";
    return send_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, mna);
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
