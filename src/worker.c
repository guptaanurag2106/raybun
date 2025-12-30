#include <cJSON.h>
#include <curl/curl.h>
#include <microhttpd.h>  // Included in case worker needs to listen
#include <stdio.h>
#include <stdlib.h>

#include "api.h"
#include "utils.h"

// Helper to perform HTTP GET/POST
// static char* http_request(const char* url, const char* json_body);

bool worker_connect(const char *master_ip, int port) {
    Log(Log_Info,
        temp_sprintf("Worker: Connecting to Master at %s:%d", master_ip, port));

    // TODO:
    // 1. Init Curl
    // 2. GET /api/scene -> load_scene()
    // 3. POST /api/register -> Send capabilities
    // 4. Loop:
    //    - GET /api/work
    //    - Render Tile
    //    - POST /api/result
    UNUSED(worker_daemon);
    return false;
}
