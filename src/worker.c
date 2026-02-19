#include <cJSON.h>
#include <curl/curl.h>
#include <time.h>

#include "api.h"
#include "renderer.h"
#include "scene.h"
#include "utils.h"

struct CurlBuffer {
    char *data;
    size_t size;
};

static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *ud) {
    size_t realsz = size * nmemb;
    struct CurlBuffer *b = (struct CurlBuffer *)ud;
    char *newbuf = realloc(b->data, b->size + realsz + 1);
    if (!newbuf) return 0;
    b->data = newbuf;
    memcpy(b->data + b->size, ptr, realsz);
    b->size += realsz;
    b->data[b->size] = '\0';
    return realsz;
}

static char *http_get(CURL *curl, const char *url) {
    struct CurlBuffer buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        free(buf.data);
        return NULL;
    }
    return buf.data;
}

static char *http_post(CURL *curl, const char *url, const char *body,
                       struct curl_slist *headers) {
    struct CurlBuffer buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        free(buf.data);
        return NULL;
    }
    return buf.data;
}

bool worker_connect(const char *master_ip, int port, MachineInfo stats) {
    Log(Log_Info,
        temp_sprintf("Worker: Connecting to Master at %s:%d", master_ip, port));

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    char base[512];
    if (strstr(master_ip, "http://") == master_ip ||
        strstr(master_ip, "https://") == master_ip) {
        snprintf(base, sizeof(base), "%s", master_ip);
    } else {
        snprintf(base, sizeof(base), "http://%s:%d", master_ip, port);
    }

    // 1) GET /api/scene
    char scene_url[512];
    snprintf(scene_url, sizeof(scene_url), "%s/api/scene", base);
    char *scene_resp = http_get(curl, scene_url);
    if (!scene_resp) {
        Log(Log_Error, "Worker: Failed to GET /api/scene");
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return false;
    }

    cJSON *root = cJSON_Parse(scene_resp);
    if (!root) {
        Log(Log_Error, "Worker: Invalid JSON from /api/scene");
        free(scene_resp);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return false;
    }
    cJSON *scene_json = cJSON_GetObjectItemCaseSensitive(root, "scene_json");
    cJSON *scene_crc_j = cJSON_GetObjectItemCaseSensitive(root, "scene_crc");
    if (!cJSON_IsString(scene_json)) {
        Log(Log_Error, "Worker: /api/scene missing scene_json");
        cJSON_Delete(root);
        free(scene_resp);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return false;
    }

    // Load scene into local Scene and State
    Scene *scene = malloc(sizeof(Scene));
    memset(scene, 0, sizeof(Scene));
    scene->arena = arena_create(1024 * 1024 * 128);
    State *state = malloc(sizeof(State));

    load_scene(scene_json->valuestring, scene, state);
    scene->scene_json = strdup(scene_json->valuestring);
    if (cJSON_IsNumber(scene_crc_j)) {
        scene->scene_crc = (unsigned int)scene_crc_j->valueint;
    }

    cJSON_Delete(root);
    free(scene_resp);

    // 2) Register (best-effort)
    char reg_url[512];
    snprintf(reg_url, sizeof(reg_url), "%s/api/register", base);
    cJSON *reg = cJSON_CreateObject();
    cJSON_AddStringToObject(reg, "name", stats.name);
    cJSON_AddNumberToObject(reg, "perf", stats.perf);
    cJSON_AddNumberToObject(reg, "thread_count", stats.thread_count);
    cJSON_AddNumberToObject(reg, "simd", stats.simd);
    char *reg_body = cJSON_PrintUnformatted(reg);
    cJSON_Delete(reg);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    // NOTE: no timeouts or retries configured on CURL requests
    char *reg_resp = http_post(curl, reg_url, reg_body, headers);
    if (reg_resp) {
        Log(Log_Info, "Worker: Registered with master");
        free(reg_resp);
    } else {
        Log(Log_Warn, "Worker: Registration failed (continuing)");
    }
    free(reg_body);

    // 3) Work loop
    while (true) {
        char work_url[512];
        // TODO: pass a unique worker_id instead of the literal "worker"
        snprintf(work_url, sizeof(work_url),
                 "%s/api/work?worker_id=%s&scene_crc=%u", base, stats.name,
                 scene->scene_crc);
        char *work_resp = http_get(curl, work_url);
        if (!work_resp) break;

        cJSON *workj = cJSON_Parse(work_resp);
        free(work_resp);
        if (!workj) break;

        if (cJSON_GetObjectItemCaseSensitive(workj, "status")) {
            // no tiles left or error
            cJSON_Delete(workj);
            break;
        }

        cJSON *tilej = cJSON_GetObjectItemCaseSensitive(workj, "tile");
        cJSON *tile_id_j = cJSON_GetObjectItemCaseSensitive(workj, "tile_id");
        if (!tilej || !tile_id_j) {
            cJSON_Delete(workj);
            break;
        }

        Tile tile = {0};
        tile.x = cJSON_GetObjectItemCaseSensitive(tilej, "x")->valueint;
        tile.y = cJSON_GetObjectItemCaseSensitive(tilej, "y")->valueint;
        tile.tw = cJSON_GetObjectItemCaseSensitive(tilej, "tw")->valueint;
        tile.th = cJSON_GetObjectItemCaseSensitive(tilej, "th")->valueint;
        int tid = tile_id_j->valueint;
        cJSON_Delete(workj);

        uint32_t *buf = malloc(tile.tw * tile.th * sizeof(uint32_t));
        if (!buf) break;

        // compute camera-derived vectors and render the tile
        Camera cam = scene->camera;
        V3f pixel00_loc, pixel_delta_u, pixel_delta_v, defocus_disk_u,
            defocus_disk_v;
        compute_render_camera_fields(
            &cam, state->width, state->height, &pixel00_loc, &pixel_delta_u,
            &pixel_delta_v, &defocus_disk_u, &defocus_disk_v);

        render_single_tile(scene, &tile, &cam, state->samples_per_pixel,
                           state->max_depth, &pixel00_loc, &pixel_delta_u,
                           &pixel_delta_v, &defocus_disk_u, &defocus_disk_v,
                           1.0f / state->samples_per_pixel, state->width, buf);

        // build hex payload (inefficient - consider binary POST)
        int pixel_count = tile.tw * tile.th;
        int hex_len = pixel_count * 8;
        char *hex = malloc(hex_len + 1);
        char *hp = hex;
        for (int i = 0; i < pixel_count; i++) {
            sprintf(hp, "%08x", buf[i]);
            hp += 8;
        }
        hex[hex_len] = '\0';

        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "name", "worker");
        cJSON_AddNumberToObject(res, "tile_id", tid);
        cJSON_AddStringToObject(res, "pixels", hex);
        char *res_body = cJSON_PrintUnformatted(res);
        cJSON_Delete(res);
        free(hex);
        free(buf);

        char res_url[512];
        snprintf(res_url, sizeof(res_url), "%s/api/result", base);
        char *res_resp = http_post(curl, res_url, res_body, headers);
        free(res_body);
        free(res_resp);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return true;
}
