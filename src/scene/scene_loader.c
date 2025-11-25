#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#include "scene.h"
#include "utils.h"
#include "vec.h"

void log_warn(const char *msg) {
    Log(Log_Warn, temp_sprintf("load_scene: %s", msg));
}

void print_summary(JSON res) {
    Log(Log_Info, temp_sprintf("load_scene: Creating image of size %d x %d",
                               res.state.width, res.state.height));
    Log(Log_Info,
        temp_sprintf("load_scene: loaded %d spheres", res.scene.sphere_count));
    Log(Log_Info,
        temp_sprintf("load_scene: loaded %d planes", res.scene.plane_count));
    Log(Log_Info, temp_sprintf("load_scene: loaded %d materials",
                               res.scene.material_count));
}

JSON load_scene(const char *scene_file) {
    const char *file = read_entire_file(scene_file);
    if (file == NULL) {
        exit(1);
    }

    cJSON *json = cJSON_Parse(file);
    free((void *)file);

    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            Log(Log_Error, temp_sprintf("load_scene: %s", error_ptr));
        }
        cJSON_Delete(json);
        exit(1);
    }

    Scene scene = {
        .plane_count = 0, .planes = NULL, .sphere_count = 0, .spheres = NULL};
    State state = {.width = 1024, .height = 768};
    Camera camera = {.position = {0, 0, -5}, .look_at = {0, 0, 0}, .fov = 60};

    // Parse config (if exists)
    cJSON *config = cJSON_GetObjectItemCaseSensitive(json, "config");
    if (cJSON_IsObject(config)) {
        cJSON *width = cJSON_GetObjectItemCaseSensitive(config, "width");
        if (cJSON_IsNumber(width)) {
            state.width = width->valueint;
        } else {
            log_warn(
                "Expected 'width' to be a number in 'config', using default.");
        }

        cJSON *height = cJSON_GetObjectItemCaseSensitive(config, "height");
        if (cJSON_IsNumber(height)) {
            state.height = height->valueint;
        } else {
            log_warn(
                "Expected 'height' to be a number in 'config', using default.");
        }
    } else {
        log_warn("'config' section not found, using default values.");
    }

    // Parse camera section (if exists)
    cJSON *cam = cJSON_GetObjectItemCaseSensitive(json, "camera");
    if (cJSON_IsObject(cam)) {
        cJSON *fov = cJSON_GetObjectItemCaseSensitive(cam, "fov");
        if (cJSON_IsNumber(fov)) {
            camera.fov = fov->valuedouble;
        } else {
            log_warn(
                "Expected 'fov' to be a number in 'camera', using default.");
        }

        cJSON *position = cJSON_GetObjectItemCaseSensitive(cam, "position");
        if (cJSON_IsArray(position) && cJSON_GetArraySize(position) == 3) {
            camera.position =
                (v3f){cJSON_GetArrayItem(position, 0)->valuedouble,
                      cJSON_GetArrayItem(position, 1)->valuedouble,
                      cJSON_GetArrayItem(position, 2)->valuedouble};
        } else {
            log_warn(
                "Expected 'position' to be an array of 3 numbers in 'camera', "
                "using default.");
        }

        cJSON *look_at = cJSON_GetObjectItemCaseSensitive(cam, "look_at");
        if (cJSON_IsArray(look_at) && cJSON_GetArraySize(look_at) == 3) {
            camera.look_at = (v3f){cJSON_GetArrayItem(look_at, 0)->valuedouble,
                                   cJSON_GetArrayItem(look_at, 1)->valuedouble,
                                   cJSON_GetArrayItem(look_at, 2)->valuedouble};
        } else {
            log_warn(
                "Expected 'look_at' to be an array of 3 numbers in 'camera', "
                "using default.");
        }
    } else {
        log_warn("'camera' section not found, using default values.");
    }

    // Parse objects section (spheres and planes)
    cJSON *objects = cJSON_GetObjectItemCaseSensitive(json, "objects");
    if (cJSON_IsObject(objects)) {
        cJSON *sphere_items =
            cJSON_GetObjectItemCaseSensitive(objects, "sphere");
        if (cJSON_IsArray(sphere_items)) {
            int total_spheres = cJSON_GetArraySize(sphere_items);
            Sphere *temp_spheres =
                malloc(sizeof(Sphere) * total_spheres);  // Temporary allocation

            int valid_spheres = 0;
            for (int i = 0; i < total_spheres; i++) {
                cJSON *sphere = cJSON_GetArrayItem(sphere_items, i);
                cJSON *center =
                    cJSON_GetObjectItemCaseSensitive(sphere, "center");
                cJSON *radius =
                    cJSON_GetObjectItemCaseSensitive(sphere, "radius");

                if (cJSON_IsArray(center) && cJSON_GetArraySize(center) == 3 &&
                    cJSON_IsNumber(radius)) {
                    temp_spheres[valid_spheres].center =
                        (v3f){cJSON_GetArrayItem(center, 0)->valuedouble,
                              cJSON_GetArrayItem(center, 1)->valuedouble,
                              cJSON_GetArrayItem(center, 2)->valuedouble};
                    temp_spheres[valid_spheres].radius = radius->valuedouble;
                    valid_spheres++;
                } else {
                    log_warn("Invalid 'sphere' entry, skipping.");
                }
            }

            scene.sphere_count = valid_spheres;
            if (valid_spheres > 0) {
                scene.spheres =
                    realloc(temp_spheres, sizeof(Sphere) * valid_spheres);
            } else {
                free(temp_spheres);
                scene.spheres = NULL;
            }
        } else {
            log_warn("'sphere' should be an array in 'objects', skipping.");
        }

        cJSON *plane_items = cJSON_GetObjectItemCaseSensitive(objects, "plane");
        if (cJSON_IsArray(plane_items)) {
            int total_planes = cJSON_GetArraySize(plane_items);
            Plane *temp_planes =
                malloc(sizeof(Plane) * total_planes);  // Temporary allocation

            int valid_planes = 0;
            for (int i = 0; i < total_planes; i++) {
                cJSON *plane = cJSON_GetArrayItem(plane_items, i);
                cJSON *normal =
                    cJSON_GetObjectItemCaseSensitive(plane, "normal");

                if (cJSON_IsArray(normal) && cJSON_GetArraySize(normal) == 3) {
                    temp_planes[valid_planes].normal =
                        (v3f){cJSON_GetArrayItem(normal, 0)->valuedouble,
                              cJSON_GetArrayItem(normal, 1)->valuedouble,
                              cJSON_GetArrayItem(normal, 2)->valuedouble};
                    valid_planes++;
                } else {
                    log_warn("Invalid 'plane' entry, skipping.");
                }
            }

            scene.plane_count = valid_planes;
            if (valid_planes > 0) {
                scene.planes =
                    realloc(temp_planes, sizeof(Plane) * valid_planes);
            } else {
                free(temp_planes);
                scene.planes = NULL;
            }
        } else {
            log_warn("'plane' should be an array in 'objects', skipping.");
        }
    } else {
        log_warn("'objects' section not found or malformed.");
    }

    cJSON_Delete(json);

    state.image =
        (uint32_t *)malloc(state.width * state.height * sizeof(*state.image));
    if (state.image == NULL) {
        Log(Log_Error, temp_sprintf("load_scene: could not allocate image: %s",
                                    strerror(errno)));
        exit(1);
    }

    JSON res = {.scene = scene, .state = state, .camera = camera};
    Log(Log_Info, temp_sprintf("load_scene: Successfully loaded %s, summary: ",
                               scene_file));
    print_summary(res);

    return res;
}
