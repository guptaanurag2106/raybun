#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "scene.h"
#include "utils.h"

JSON load_scene(const char *scene_file) {
    const char *file = read_entire_file(scene_file);
    if (file == NULL) {
        exit(1);
    }

    cJSON *json = cJSON_Parse(file);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            Log(Log_Error, temp_sprintf("load_scene: %s", error_ptr));
        }
        cJSON_Delete(json);
        exit(1);
    }

    Scene scene = {0};
    State state = {0};

    state.width = 1024;
    state.height = 768;

    state.image =
        (uint32_t *)malloc(state.width * state.height * sizeof(*state.image));
    if (state.image == NULL) {
        Log(Log_Error, temp_sprintf("load_scene: could not allocate image: %s",
                                    strerror(errno)));
        exit(1);
    }
    JSON res = {.scene = scene, .state = state};
    return res;
}
