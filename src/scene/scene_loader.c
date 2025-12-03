#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
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
        temp_sprintf("load_scene: Loaded %d spheres", res.scene.sphere_count));
    Log(Log_Info,
        temp_sprintf("load_scene: Loaded %d planes", res.scene.plane_count));
    Log(Log_Info,
        temp_sprintf("load_scene: Loaded %d quads", res.scene.quad_count));
    Log(Log_Info, temp_sprintf("load_scene: Loaded %d materials",
                               res.scene.material_count));
}

// TODO: check what all actually needs to be normalized
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

    Scene scene = {.plane_count = 0,
                   .planes = NULL,
                   .sphere_count = 0,
                   .spheres = NULL,
                   .quad_count = 0,
                   .quads = 0,
                   .material_count = 0,
                   .materials = NULL};
    State state = {
        .width = 1024, .height = 768, .samples_per_pixel = 10, .max_depth = 10};
    Camera camera = {.position = {0, 0, -5},
                     .look_at = {0, 0, 0},
                     .up = {0, 1, 0},
                     .fov = DEG2RAD(60),
                     .aspect_ratio = 4.0f / 3,
                     .defocus_angle = 0,
                     .focus_dist = 1};

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

        cJSON *samples_per_pixel =
            cJSON_GetObjectItemCaseSensitive(config, "samples_per_pixel");
        if (cJSON_IsNumber(samples_per_pixel)) {
            state.samples_per_pixel = samples_per_pixel->valueint;
        } else {
            log_warn(
                "Expected 'samples_per_pixel' to be a number in 'config', "
                "using default.");
        }

        cJSON *max_depth =
            cJSON_GetObjectItemCaseSensitive(config, "max_depth");
        if (cJSON_IsNumber(max_depth)) {
            state.max_depth = max_depth->valueint;
        } else {
            log_warn(
                "Expected 'max_depth' to be a number in 'config', "
                "using default.");
        }
    } else {
        log_warn("'config' section not found, using default values.");
    }

    // Parse camera section (if exists)
    cJSON *cam = cJSON_GetObjectItemCaseSensitive(json, "camera");
    if (cJSON_IsObject(cam)) {
        cJSON *fov = cJSON_GetObjectItemCaseSensitive(cam, "fov");
        if (cJSON_IsNumber(fov)) {
            camera.fov = DEG2RAD(fov->valuedouble);
        } else {
            log_warn(
                "Expected 'fov' to be a number (degrees) in 'camera', using "
                "default.");
        }
        cJSON *defocus_angle =
            cJSON_GetObjectItemCaseSensitive(cam, "defocus_angle");
        if (cJSON_IsNumber(defocus_angle)) {
            camera.defocus_angle = DEG2RAD(defocus_angle->valuedouble);
        } else {
            log_warn(
                "Expected 'defocus_angle' to be a number (degrees) in "
                "'camera', using default.");
        }

        cJSON *focus_dist = cJSON_GetObjectItemCaseSensitive(cam, "focus_dist");
        if (cJSON_IsNumber(focus_dist)) {
            camera.focus_dist = focus_dist->valuedouble;
        } else {
            log_warn(
                "Expected 'focus_dist' to be a number in 'camera', using "
                "default.");
        }

        cJSON *aspect_ratio =
            cJSON_GetObjectItemCaseSensitive(cam, "aspect_ratio");
        if (cJSON_IsString(aspect_ratio)) {
            const char *s = aspect_ratio->valuestring;
            int num, den;

            if (sscanf(s, "%d/%d", &num, &den) == 2 && den != 0) {
                camera.aspect_ratio = (float)num / den;
            } else {
                log_warn(
                    "Expected 'aspect_ratio' to be a string (fraction) in "
                    "'camera', using default.");
            }
        } else {
            log_warn(
                "Expected 'aspect_ratio' to be a string (fraction) in "
                "'camera', using default.");
        }

        cJSON *position = cJSON_GetObjectItemCaseSensitive(cam, "position");
        if (cJSON_IsArray(position) && cJSON_GetArraySize(position) == 3) {
            camera.position =
                (V3f){cJSON_GetArrayItem(position, 0)->valuedouble,
                      cJSON_GetArrayItem(position, 1)->valuedouble,
                      cJSON_GetArrayItem(position, 2)->valuedouble};
        } else {
            log_warn(
                "Expected 'position' to be an array of 3 numbers in 'camera', "
                "using default.");
        }

        cJSON *look_at = cJSON_GetObjectItemCaseSensitive(cam, "look_at");
        if (cJSON_IsArray(look_at) && cJSON_GetArraySize(look_at) == 3) {
            camera.look_at = v3f_normalize(
                (V3f){cJSON_GetArrayItem(look_at, 0)->valuedouble,
                      cJSON_GetArrayItem(look_at, 1)->valuedouble,
                      cJSON_GetArrayItem(look_at, 2)->valuedouble});
        } else {
            log_warn(
                "Expected 'look_at' to be an array of 3 numbers in 'camera', "
                "using default.");
        }

        cJSON *up = cJSON_GetObjectItemCaseSensitive(cam, "up");
        if (cJSON_IsArray(up) && cJSON_GetArraySize(up) == 3) {
            camera.up =
                v3f_normalize((V3f){cJSON_GetArrayItem(up, 0)->valuedouble,
                                    cJSON_GetArrayItem(up, 1)->valuedouble,
                                    cJSON_GetArrayItem(up, 2)->valuedouble});
        } else {
            log_warn(
                "Expected 'up' to be an array of 3 numbers in 'camera', "
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
        if (cJSON_IsArray(sphere_items) &&
            cJSON_GetArraySize(sphere_items) > 0) {
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
                cJSON *mat_index =
                    cJSON_GetObjectItemCaseSensitive(sphere, "material");

                if (cJSON_IsArray(center) && cJSON_IsNumber(mat_index) &&
                    cJSON_GetArraySize(center) == 3 && cJSON_IsNumber(radius) &&
                    radius->valuedouble > 0) {
                    temp_spheres[valid_spheres].mat_index = mat_index->valueint;
                    temp_spheres[valid_spheres].center =
                        (V3f){cJSON_GetArrayItem(center, 0)->valuedouble,
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
            if (sphere_items != NULL)
                log_warn("'sphere' should be an array in 'objects', skipping.");
        }

        cJSON *plane_items = cJSON_GetObjectItemCaseSensitive(objects, "plane");
        if (cJSON_IsArray(plane_items) && cJSON_GetArraySize(plane_items) > 0) {
            int total_planes = cJSON_GetArraySize(plane_items);
            Plane *temp_planes =
                malloc(sizeof(Plane) * total_planes);  // Temporary allocation

            int valid_planes = 0;
            for (int i = 0; i < total_planes; i++) {
                cJSON *plane = cJSON_GetArrayItem(plane_items, i);
                cJSON *normal =
                    cJSON_GetObjectItemCaseSensitive(plane, "normal");
                cJSON *point = cJSON_GetObjectItemCaseSensitive(plane, "point");
                cJSON *mat_index =
                    cJSON_GetObjectItemCaseSensitive(plane, "material");

                if (cJSON_IsArray(normal) && cJSON_IsArray(point) &&
                    cJSON_IsNumber(mat_index) &&
                    cJSON_GetArraySize(normal) == 3 &&
                    cJSON_GetArraySize(point) == 3) {
                    temp_planes[valid_planes].mat_index = mat_index->valueint;
                    temp_planes[valid_planes].normal = v3f_normalize(
                        (V3f){cJSON_GetArrayItem(normal, 0)->valuedouble,
                              cJSON_GetArrayItem(normal, 1)->valuedouble,
                              cJSON_GetArrayItem(normal, 2)->valuedouble});
                    temp_planes[valid_planes].point =
                        (V3f){cJSON_GetArrayItem(point, 0)->valuedouble,
                              cJSON_GetArrayItem(point, 1)->valuedouble,
                              cJSON_GetArrayItem(point, 2)->valuedouble};
                    temp_planes[valid_planes].d =
                        v3f_dot(temp_planes[valid_planes].normal,
                                temp_planes[valid_planes].point);
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
            if (plane_items != NULL)
                log_warn("'plane' should be an array in 'objects', skipping.");
        }

        cJSON *quad_items = cJSON_GetObjectItemCaseSensitive(objects, "quad");
        if (cJSON_IsArray(quad_items) && cJSON_GetArraySize(quad_items)) {
            int total_quads = cJSON_GetArraySize(quad_items);
            Quad *temp_quads =
                malloc(sizeof(Quad) * total_quads);  // Temporary allocation

            int valid_quads = 0;
            for (int i = 0; i < total_quads; i++) {
                cJSON *quad = cJSON_GetArrayItem(quad_items, i);
                cJSON *corner =
                    cJSON_GetObjectItemCaseSensitive(quad, "corner");
                cJSON *u = cJSON_GetObjectItemCaseSensitive(quad, "u");
                cJSON *v = cJSON_GetObjectItemCaseSensitive(quad, "v");
                cJSON *mat_index =
                    cJSON_GetObjectItemCaseSensitive(quad, "material");

                if (cJSON_IsArray(corner) && cJSON_IsNumber(mat_index) &&
                    cJSON_GetArraySize(corner) == 3 && cJSON_IsArray(u) &&
                    cJSON_GetArraySize(u) == 3 && cJSON_IsArray(v) &&
                    cJSON_GetArraySize(v) == 3) {
                    temp_quads[valid_quads].mat_index = mat_index->valueint;
                    temp_quads[valid_quads].corner =
                        (V3f){cJSON_GetArrayItem(corner, 0)->valuedouble,
                              cJSON_GetArrayItem(corner, 1)->valuedouble,
                              cJSON_GetArrayItem(corner, 2)->valuedouble};
                    temp_quads[valid_quads].u =
                        (V3f){cJSON_GetArrayItem(u, 0)->valuedouble,
                              cJSON_GetArrayItem(u, 1)->valuedouble,
                              cJSON_GetArrayItem(u, 2)->valuedouble};
                    temp_quads[valid_quads].v =
                        (V3f){cJSON_GetArrayItem(v, 0)->valuedouble,
                              cJSON_GetArrayItem(v, 1)->valuedouble,
                              cJSON_GetArrayItem(v, 2)->valuedouble};
                    temp_quads[valid_quads].normal = v3f_cross(
                        temp_quads[valid_quads].u, temp_quads[valid_quads].v);
                    const V3f nn =
                        v3f_normalize(temp_quads[valid_quads].normal);
                    temp_quads[valid_quads].d =
                        v3f_dot(nn, temp_quads[valid_quads].corner);
                    temp_quads[valid_quads].w =
                        v3f_divf(temp_quads[valid_quads].normal,
                                 v3f_dot(temp_quads[valid_quads].normal,
                                         temp_quads[valid_quads].normal));
                    temp_quads[valid_quads].normal = nn;
                    valid_quads++;
                } else {
                    log_warn("Invalid 'quad' entry, skipping.");
                }
            }

            scene.quad_count = valid_quads;
            if (valid_quads > 0) {
                scene.quads = realloc(temp_quads, sizeof(Quad) * valid_quads);
            } else {
                free(temp_quads);
                scene.quads = NULL;
            }
        } else {
            if (quad_items != NULL)
                log_warn("'quad' should be an array in 'objects', skipping.");
        }

    } else {
        log_warn("'objects' section not found or malformed.");
    }

    cJSON *materials = cJSON_GetObjectItemCaseSensitive(json, "materials");
    if (cJSON_IsArray(materials) && cJSON_GetArraySize(materials)) {
        int total_mat = cJSON_GetArraySize(materials);
        Material *temp_materials =
            malloc(sizeof(Material) * total_mat);  // Temporary allocation

        int valid_materials = 0;
        for (int i = 0; i < total_mat; i++) {
            cJSON *material = cJSON_GetArrayItem(materials, i);
            cJSON *type = cJSON_GetObjectItemCaseSensitive(material, "type");
            cJSON *albedo =
                cJSON_GetObjectItemCaseSensitive(material, "albedo");
            cJSON *emission =
                cJSON_GetObjectItemCaseSensitive(material, "emission");

            if (cJSON_IsString(type)) {
                MaterialType mat_type = mat_to_string(type->valuestring);
                temp_materials[valid_materials].type = mat_type;
                if (mat_type == MAT_LAMBERTIAN && cJSON_IsArray(albedo) &&
                    cJSON_GetArraySize(albedo) == 3) {
                    temp_materials[valid_materials]
                        .properties.lambertian.albedo =

                        (V3f){cJSON_GetArrayItem(albedo, 0)->valuedouble,
                              cJSON_GetArrayItem(albedo, 1)->valuedouble,
                              cJSON_GetArrayItem(albedo, 2)->valuedouble};
                } else if (mat_type == MAT_METAL && cJSON_IsArray(albedo) &&
                           cJSON_GetArraySize(albedo) == 3) {
                    cJSON *jfuzz =
                        cJSON_GetObjectItemCaseSensitive(material, "fuzz");
                    float fuzz = 0;
                    if (cJSON_IsNumber(jfuzz)) {
                        fuzz = jfuzz->valuedouble;
                        if (fuzz < 0 || fuzz > 1) {
                            fuzz = clamp_float(fuzz, 0, 1);
                            log_warn(
                                "Invalid 'fuzz' entry, must be between 0, 1, "
                                "using default.");
                        }
                    }
                    temp_materials[valid_materials].properties.metal.albedo =

                        (V3f){cJSON_GetArrayItem(albedo, 0)->valuedouble,
                              cJSON_GetArrayItem(albedo, 1)->valuedouble,
                              cJSON_GetArrayItem(albedo, 2)->valuedouble};
                    temp_materials[valid_materials].properties.metal.fuzz =
                        fuzz;
                } else if (mat_type == MAT_EMISSIVE &&
                           cJSON_IsArray(emission) &&
                           cJSON_GetArraySize(emission) == 3) {
                    temp_materials[valid_materials]
                        .properties.emissive.emission =
                        (V3f){cJSON_GetArrayItem(emission, 0)->valuedouble,
                              cJSON_GetArrayItem(emission, 1)->valuedouble,
                              cJSON_GetArrayItem(emission, 2)->valuedouble};
                } else if (mat_type == MAT_DIELECTRIC) {
                    cJSON *jetai_eta = cJSON_GetObjectItemCaseSensitive(
                        material, "refraction_index");
                    if (cJSON_IsNumber(jetai_eta)) {
                        temp_materials[valid_materials]
                            .properties.dielectric.etai_eta =
                            jetai_eta->valuedouble;
                    } else {
                        log_warn(
                            "Invalid 'refractive_index' entry using default.");
                        temp_materials[valid_materials]
                            .properties.dielectric.etai_eta = 1;
                    }
                } else {
                    log_warn("Unkown 'type', skipping.");
                }
                valid_materials++;
            } else {
                log_warn("Invalid 'material' entry, skipping.");
            }
        }

        scene.material_count = valid_materials;
        if (valid_materials > 0) {
            scene.materials =
                realloc(temp_materials, sizeof(Material) * valid_materials);
        } else {
            free(temp_materials);
            scene.materials = NULL;
        }
    } else {
        if (materials != NULL)
            log_warn("'material' should be an array in 'objects', skipping.");
    }

    cJSON_Delete(json);

    state.image =
        (uint32_t *)malloc(state.width * state.height * sizeof(*state.image));
    if (state.image == NULL) {
        Log(Log_Error, temp_sprintf("load_scene: could not allocate image: %s",
                                    strerror(errno)));
        exit(1);
    }
    scene.camera = camera;

    JSON res = {.scene = scene, .state = state};
    Log(Log_Info,
        temp_sprintf("load_scene: Successfully loaded %s", scene_file));

    return res;
}
