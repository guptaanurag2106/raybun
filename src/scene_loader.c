#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "aabb.h"
#include "common.h"
#include "json.h"
#include "rinternal.h"
#include "utils.h"
#include "vec.h"

static void fatal(const char *msg) {
    Log(Log_Error, msg);
    exit(1);
}

static void log_warn(const char *msg) { Log(Log_Warn, "load_scene: %s", msg); }

void print_summary(const Scene *scene, const State *state) {
    Log(Log_Info, "load_scene: Creating image of size %d x %d", state->width,
        state->height);
    Log(Log_Info, "load_scene: Loaded %d spheres", scene->sphere_count);
    Log(Log_Info, "load_scene: Loaded %d planes", scene->plane_count);
    Log(Log_Info, "load_scene: Loaded %d triangles", scene->triangle_count);
    Log(Log_Info, "load_scene: Loaded %d quads", scene->quad_count);
    Log(Log_Info, "load_scene: Loaded %d materials", scene->materials.size);
}

// TODO: check what all actually needs to be normalized
static double json_value_number(const Json_Value *value) {
    if (value == NULL) return 0.0;
    return value->data_kind == JSON_INTEGER ? value->data.intval
                                            : value->data.realval;
}

static V3f parse_v3f(const Json *arr, const char *ctx, const V3f fallback) {
    if (!json_is_array(arr) || json_array_size(arr) != 3) {
        log_warn(temp_sprintf("%s: expected array[3], using default.", ctx));
        return fallback;
    }

    const Json_Value *x = json_array_at(arr, 0);
    const Json_Value *y = json_array_at(arr, 1);
    const Json_Value *z = json_array_at(arr, 2);
    if ((x->data_kind != JSON_INTEGER && x->data_kind != JSON_REAL) ||
        (y->data_kind != JSON_INTEGER && y->data_kind != JSON_REAL) ||
        (z->data_kind != JSON_INTEGER && z->data_kind != JSON_REAL)) {
        log_warn(
            temp_sprintf("%s: expected numeric array[3], using default.", ctx));
        return fallback;
    }

    return (V3f){(float)json_value_number(x), (float)json_value_number(y),
                 (float)json_value_number(z)};
}

static float parse_float(const Json *node, const char *ctx, float fallback) {
    if (!json_is_number(node)) {
        log_warn(temp_sprintf("%s: expected number, using default.", ctx));
        return fallback;
    }
    return (float)json_number(node);
}

static int parse_int(const Json *node, const char *ctx, int fallback) {
    if (!json_is_integer(node)) {
        log_warn(temp_sprintf("%s: expected integer, using default.", ctx));
        return fallback;
    }
    return (int)json_integer(node);
}

static int parse_mat_index(const Json *node, size_t mat_count,
                           const char *ctx) {
    if (!json_is_integer(node) || json_integer(node) < 0 ||
        json_integer(node) >= (int)mat_count) {
        log_warn(temp_sprintf("%s: invalid material index.", ctx));
        return -1;
    }
    return (int)json_integer(node);
}

static char *parse_string(const Json *node, const char *ctx) {
    if (!json_is_string(node)) {
        log_warn(temp_sprintf("%s: expected string.", ctx));
        return NULL;
    }

    return json_cstring(node);
}

static Quad make_quad(V3f corner, V3f u, V3f v, int mat_index) {
    Quad q = {0};
    q.corner = corner;
    q.u = u;
    q.v = v;
    q.mat_index = mat_index;

    const V3f n = v3f_cross(u, v);
    const float L = v3f_slength(n);
    const V3f nn = v3f_mulf(n, 1.0f / sqrtf(L));

    q.normal = nn;
    q.d = v3f_dot(nn, corner);
    q.w = v3f_mulf(n, 1.0f / L);
    return q;
}

// considering p1 as the common "corner"
static Triangle make_triangle(V3f p1, V3f p2, V3f p3, V3f n1, V3f n2, V3f n3,
                              V2f uv1, V2f uv2, V2f uv3, int mat_index) {
    V3f e1 = v3f_sub(p2, p1);
    V3f e2 = v3f_sub(p3, p1);
    Vertex v1 = {p1, n1, uv1};
    Vertex v2 = {p2, n2, uv2};
    Vertex v3 = {p3, n3, uv3};
    return (Triangle){.v1 = v1,
                      .v2 = v2,
                      .v3 = v3,
                      .e1 = e1,
                      .e2 = e2,
                      .mat_index = mat_index};
}

static inline void append_hittable(Scene *scene, Hittable h) {
    vec_push(&scene->objects, h);
}

static void append_sphere(Scene *scene, Sphere sphere) {
    scene->sphere_count++;
    Sphere *sphere_data = arena_alloc_struct(&scene->arena, Sphere);
    *sphere_data = sphere;

    Hittable h = make_hittable_sphere(sphere_data);
    append_hittable(scene, h);
}

static void append_plane(Scene *scene, Plane plane) {
    scene->plane_count++;
    Plane *plane_data = arena_alloc_struct(&scene->arena, Plane);
    *plane_data = plane;
    Hittable h = make_hittable_plane(plane_data);
    append_hittable(scene, h);
}

static void append_triangle(Scene *scene, Triangle triangle) {
    scene->triangle_count++;
    Triangle *triangle_data = arena_alloc_struct(&scene->arena, Triangle);
    *triangle_data = triangle;
    Hittable h = make_hittable_triangle(triangle_data);
    append_hittable(scene, h);
}

static void append_quad(Scene *scene, Quad quad) {
    scene->quad_count++;
    Quad *quad_data = arena_alloc_struct(&scene->arena, Quad);
    *quad_data = quad;
    Hittable h = make_hittable_quad(quad_data);
    append_hittable(scene, h);
}

// TODO: currently axis aligned box support transformations
static void add_box(Scene *scene, V3f a, V3f b, int mat_index) {
    const V3f minp = {MIN(a.x, b.x), MIN(a.y, b.y), MIN(a.z, b.z)};

    const V3f maxp = {MAX(a.x, b.x), MAX(a.y, b.y), MAX(a.z, b.z)};

    const V3f dx = {maxp.x - minp.x, 0, 0};
    const V3f dy = {0, maxp.y - minp.y, 0};
    const V3f dz = {0, 0, maxp.z - minp.z};

    // front
    append_quad(scene,
                make_quad((V3f){minp.x, minp.y, maxp.z}, dy, dx, mat_index));

    // right
    append_quad(scene, make_quad((V3f){maxp.x, minp.y, maxp.z}, dy,
                                 (V3f){-dz.x, -dz.y, -dz.z}, mat_index));

    // back
    append_quad(scene, make_quad((V3f){maxp.x, minp.y, minp.z}, dy,
                                 (V3f){-dx.x, -dx.y, -dx.z}, mat_index));

    // left
    append_quad(scene,
                make_quad((V3f){minp.x, minp.y, minp.z}, dy, dz, mat_index));

    // top
    append_quad(scene, make_quad((V3f){minp.x, maxp.y, maxp.z},
                                 (V3f){-dz.x, -dz.y, -dz.z}, dx, mat_index));

    // bottom
    append_quad(scene,
                make_quad((V3f){minp.x, minp.y, minp.z}, dz, dx, mat_index));
}

Vector(char *, MatNames);
// static void parse_mtl(const char *name, MatNames *material_names,
//                       Materials *materials) {}

static void add_model(Scene *scene, V3f position, float scale,
                      const char *file_name, Materials *scene_mats) {
    FILE *f;

    if (file_name == NULL || strlen(file_name) == 0 ||
        (f = fopen(file_name, "r")) == NULL) {
        log_warn(temp_sprintf("Cannot open file %s: %s, skipping model",
                              file_name, strerror(errno)));
        return;
    }

    char buf[512];
    size_t line_no = 0;
    size_t triangle_count = 0;

    // TODO: multiple files in mtllib, multiple mtllib?
    char *matllib = NULL;
    int curr_mat_index = 0;

    MatNames material_names = {0};
    vec_push(&material_names, "default_obj_mat");
    Material default_mat =
        (Material){.type = MAT_LAMBERTIAN,
                   .properties.lambertian.albedo =
                       (Texture){.type = TEX_CONSTANT, .colour = ORIGIN}};
    vec_push(scene_mats, default_mat);  // TODO: MAT_NONE

    Vector(V3f, Vertices);
    Vertices vs = {0};
    Vector(V3f, Textures);
    Textures vts = {0};
    Vector(V3f, Parameters);
    Parameters vps = {0};
    Vector(V3f, Normals);
    Normals vns = {0};

    while (fgets(buf, sizeof(buf), f) != NULL) {
        line_no++;
        char *ptr = buf;
        while (*ptr && isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0' || *ptr == '#') continue;

        if (*ptr == 'v') {
            ptr++;
            if (isspace((unsigned char)*ptr)) {
                // Vertex position
                float x, y, z;
                if (sscanf(ptr, "%f %f %f", &x, &y, &z) == 3) {
                    V3f vec = {x, y, z};
                    vec = v3f_add(v3f_mulf(vec, scale), position);
                    vec_push(&vs, vec);
                }
            } else if (*ptr == 't' && isspace((unsigned char)*(ptr + 1))) {
                // Texture coordinate
                float u = 0, v = 0, w = 0;
                sscanf(ptr + 1, "%f %f %f", &u, &v, &w);
                vec_push(&vts, ((V3f){u, v, w}));
            } else if (*ptr == 'n' && isspace((unsigned char)*(ptr + 1))) {
                // Normal
                float x = 0, y = 0, z = 0;
                sscanf(ptr + 1, "%f %f %f", &x, &y, &z);
                vec_push(&vns, ((V3f){x, y, z}));
            } else if (*ptr == 'p' && isspace((unsigned char)*(ptr + 1))) {
                // Parameter space
                float u = 0, v = 0, w = 0;
                sscanf(ptr + 1, "%f %f %f", &u, &v, &w);
                vec_push(&vps, ((V3f){u, v, w}));
            }
            continue;
        }

        /* if (*ptr == 'f' && isspace((unsigned char)*(ptr + 1))) { */
        /*     ptr += 2; */
        /*     int v_idx[4] = {0}; */
        /*     int count = 0; */

        /*     char *token = strtok(ptr, " \t\r\n"); */
        /*     while (token && count < 4) { */
        /*         v_idx[count] = atoi(token); */
        /*         // We could parse vt/vn here if needed, but we currently only
         */
        /*         // use vertices */
        /*         count++; */
        /*         token = strtok(NULL, " \t\r\n"); */
        /*     } */

        /*     // Convert indices (1-based to 0-based, handle negatives) */
        /*     for (int i = 0; i < count; i++) { */
        /*         if (v_idx[i] > 0) */
        /*             v_idx[i]--; */
        /*         else if (v_idx[i] < 0) */
        /*             v_idx[i] += vs.size; */
        /*         if (v_idx[i] < 0 || v_idx[i] >= (int)vs.size) */
        /*             v_idx[i] = 0;  // handling some random error case */
        /*     } */

        /*     if (count >= 3) { */
        /*         append_triangle(scene, */
        /*                         make_triangle(vec_get(&vs, v_idx[1]), */
        /*                                       vec_get(&vs, v_idx[0]), */
        /*                                       vec_get(&vs, v_idx[2]), mi));
         */
        /*         triangle_count++; */

        /*         if (count == 4) { */
        /*             append_triangle(scene, */
        /*                             make_triangle(vec_get(&vs, v_idx[2]), */
        /*                                           vec_get(&vs, v_idx[0]), */
        /*                                           vec_get(&vs, v_idx[3]),
         * mi)); */
        /*             triangle_count++; */
        /*         } */
        /*     } */
        /*     continue; */
        /* } */

        if (strncmp(ptr, "mtllib ", 7) == 0) {
            matllib = strdup(ptr + 7);
            matllib[strcspn(matllib, "\r\n")] = 0;
            // parse_mtl(matllib, &material_names, scene_mats);
            Log(Log_Info, "load_scene: mtllib using %s", matllib);
            continue;
        }

        if (strncmp(ptr, "usemtl ", 7) == 0) {
            /* char *curr_mtl = ptr + 7; */
            /* curr_mtl[strcspn(curr_mtl, "\r\n")] = 0; */

            /* Log(Log_Info, "load_scene: usemtl using %s in %s", curr_mtl, */
            /*     matllib); */
            /* size_t mat_index = */
            /*     vec_search_first(&material_names, curr_mtl, strcmp); */
            /* curr_mat_index = (int)mat_index; */
            /* if (mat_index >= materials.size) { */
            /*     Log(Log_Warn, */
            /*         "load_scene: Could not find material %s in %s, using
             * first", */
            /*         curr_mtl, matllib); */
            /*     curr_mat_index = 0; */
            /* } */

            continue;
        }

        if (strncmp(ptr, "o ", 2) == 0) {
            Log(Log_Debug, "load_scene: Loading object %s from file %s",
                ptr + 2, file_name);
            continue;
        }

        // NOTE: group<->material link efficient rendering?
        if (strncmp(ptr, "g ", 2) == 0) {
            Log(Log_Debug, "load_scene: Loading group %s from file %s", ptr + 2,
                file_name);
            continue;
        }

        // Log(Log_Warn, "load_scene: Unknown/Unsupported line '%s' in %s:%zu",
        //     buf, file_name, line_no);
    }

    Log(Log_Info, "load_scene: Loaded %s with %d vertices and %d triangles",
        file_name, vs.size, triangle_count);

    vec_free(&vs);
    vec_free(&vts);
    vec_free(&vns);
    vec_free(&vps);
    vec_free(&material_names);
    fclose(f);
}

static int parse_quad(Scene *scene, const Json_Value *qnode) {
    const Json *corner = json_value_find(qnode, "corner");
    const Json *u = json_value_find(qnode, "u");
    const Json *v = json_value_find(qnode, "v");
    const Json *mat_i = json_value_find(qnode, "material");

    int mi = parse_mat_index(mat_i, scene->materials.size, "quad.material");
    if (mi < 0) return 0;

    const V3f C = parse_v3f(corner, "quad.corner", (V3f){0});
    const V3f U = parse_v3f(u, "quad.u", (V3f){0});
    const V3f V = parse_v3f(v, "quad.v", (V3f){0});

    append_quad(scene, make_quad(C, U, V, mi));
    return 1;
}

static int parse_triangle(Scene *scene, const Json_Value *tnode) {
    const Json *p1 = json_value_find(tnode, "p1");
    const Json *p2 = json_value_find(tnode, "p2");
    const Json *p3 = json_value_find(tnode, "p3");
    const Json *mat_i = json_value_find(tnode, "material");

    int mi = parse_mat_index(mat_i, scene->materials.size, "triangle.material");
    if (mi < 0) return 0;

    const V3f P1 = parse_v3f(p1, "triangle.p1", (V3f){0});
    const V3f P2 = parse_v3f(p2, "triangle.p2", (V3f){0});
    const V3f P3 = parse_v3f(p3, "triangle.p3", (V3f){0});

    V3f e1 = v3f_sub(P2, P1);
    V3f e2 = v3f_sub(P3, P1);
    V3f n = v3f_cross(e1, e2);

    append_triangle(scene, make_triangle(P1, P2, P3, n, n, n, (V2f){0},
                                         (V2f){0}, (V2f){0}, mi));
    return 1;
}

char *read_compress_scene(const char *scene_file) {
    char *file = read_entire_file(scene_file);
    if (!file) fatal("load_scene: Cannot read file.");
    // TODO:
    // cJSON_Minify(file);
    return file;
}

void load_scene(const char *scene_file_content, Scene *scene, State *state) {
    struct timeval start, end;
    gettimeofday(&start, NULL);

    scene->objects = (Hittables){0};

    // printf("|%s|\n", scene_file_content);
    Json_Error err = {0};
    Json *json = json_parse_string(scene_file_content, &err);

    if (!json) {
        fatal(
            temp_sprintf("load_scene: JSON parse error: %s", format_json_error(&err)));
    }

    Camera camera = {.position = {0, 0, -5},
                     .look_at = {0, 0, 0},
                     .up = {0, 1, 0},
                     .fov = DEG2RAD(60),
                     .aspect_ratio = 4.f / 3.f,
                     .defocus_angle = 0,
                     .focus_dist = 1};

    const Json *config = json_find(json, "config");
    if (json_is_obj(config)) {
        int width = parse_int(json_find(config, "width"), "config.width", -1);
        int height =
            parse_int(json_find(config, "height"), "config.height", -1);
        if (width <= 0 || height <= 0) {
            fatal("config: invalid width/height");
        }
        state->width = (size_t)width;
        state->height = (size_t)height;

        state->samples_per_pixel =
            parse_int(json_find(config, "samples_per_pixel"), "config.spp",
                      state->samples_per_pixel);
        state->max_depth = parse_int(json_find(config, "max_depth"),
                                     "config.max_depth", state->max_depth);
    } else {
        fatal("config: not found.");
    }

    const Json *cam = json_find(json, "camera");
    if (json_is_obj(cam)) {
        camera.fov = DEG2RAD(parse_float(json_find(cam, "fov"), "camera.fov",
                                         RAD2DEG(camera.fov)));
        camera.defocus_angle = DEG2RAD(parse_float(
            json_find(cam, "defocus_angle"), "camera.defocus_angle", 0));
        camera.focus_dist = parse_float(json_find(cam, "focus_dist"),
                                        "camera.focus_dist", camera.focus_dist);

        // aspect ratio (fraction format)
        const Json *ar = json_find(cam, "aspect_ratio");
        if (json_is_string(ar)) {
            int n, d;
            if (sscanf(json_cstring(ar), "%d/%d", &n, &d) == 2 && d != 0)
                camera.aspect_ratio = (float)n / (float)d;
            else
                log_warn(
                    "camera.aspect_ratio: invalid fraction, using default.");
        }

        camera.position = parse_v3f(json_find(cam, "position"),
                                    "camera.position", camera.position);
        camera.look_at = parse_v3f(json_find(cam, "look_at"), "camera.look_at",
                                   camera.look_at);
        camera.up = v3f_normalize(
            parse_v3f(json_find(cam, "up"), "camera.up", camera.up));
    } else
        log_warn("camera: not found, using defaults.");

    const Json *materials = json_find(json, "materials");
    if (json_is_array(materials)) {
        size_t N = json_array_size(materials);
        scene->materials = (Materials){0};

        for (size_t i = 0; i < N; i++) {
            const Json_Value *mt = json_array_at(materials, i);
            const Json *type = json_value_find(mt, "type");
            if (!json_cstring(type)) {
                log_warn("material: missing/invalid 'type', skipping.");
                continue;
            }

            Material dst = {0};
            dst.type = string_to_mat(json_cstring(type));

            const Json *albedo = json_value_find(mt, "albedo");
            const Json *emission = json_value_find(mt, "emission");

            switch (dst.type) {
                case MAT_LAMBERTIAN:
                    dst.properties.lambertian.albedo =
                        (Texture){.type = TEX_CONSTANT,
                                  .colour = parse_v3f(albedo, "material.albedo",
                                                      (V3f){1, 1, 1})};
                    break;

                case MAT_METAL: {
                    dst.properties.metal.albedo =
                        (Texture){.type = TEX_CONSTANT,
                                  .colour = parse_v3f(albedo, "material.albedo",
                                                      (V3f){1, 1, 1})};
                    float f = parse_float(json_value_find(mt, "fuzz"),
                                          "material.fuzz", 0);
                    dst.properties.metal.fuzz = clamp_float(f, 0, 1);
                } break;

                case MAT_EMISSIVE:
                    dst.properties.emissive.emission = (Texture){
                        .type = TEX_CONSTANT,
                        .colour = parse_v3f(emission, "material.emission",
                                            (V3f){0, 0, 0})};
                    break;

                case MAT_DIELECTRIC:
                    dst.properties.dielectric.etai_eta =
                        parse_float(json_value_find(mt, "refraction_index"),
                                    "material.refraction_index", 1);
                    break;

                default:
                    Log(Log_Error, "load_scene: material: unknown type %s",
                        json_cstring(type));
                    exit(1);
            }

            vec_push(&scene->materials, dst);
        }
    }

    const Json *objects = json_find(json, "objects");
    if (!json_is_obj(objects)) {
        log_warn("objects: section missing/malformed.");
        goto END_PARSE;
    }

    const Json *sitems = json_find(objects, "sphere");
    if (json_is_array(sitems)) {
        size_t N = json_array_size(sitems);

        for (size_t i = 0; i < N; i++) {
            const Json_Value *s = json_array_at(sitems, i);
            int mi = parse_mat_index(json_value_find(s, "material"),
                                     scene->materials.size, "sphere.material");
            if (mi < 0) continue;

            const Json *center = json_value_find(s, "center");
            const Json *radius = json_value_find(s, "radius");

            Sphere sphere = {0};
            sphere.mat_index = mi;
            sphere.center = parse_v3f(center, "sphere.center", (V3f){0});
            sphere.radius = parse_float(radius, "sphere.radius", 0);
            if (sphere.radius <= 0) {
                log_warn("sphere.radius: must be >0, skipping.");
                continue;
            }

            append_sphere(scene, sphere);
        }
    }

    const Json *pitems = json_find(objects, "plane");
    if (json_is_array(pitems)) {
        size_t N = json_array_size(pitems);

        for (size_t i = 0; i < N; i++) {
            const Json_Value *p = json_array_at(pitems, i);
            int mi = parse_mat_index(json_value_find(p, "material"),
                                     scene->materials.size, "plane.material");
            if (mi < 0) continue;

            Plane plane = {0};
            plane.mat_index = mi;
            plane.normal = v3f_normalize(parse_v3f(
                json_value_find(p, "normal"), "plane.normal", (V3f){0, 1, 0}));
            plane.point = parse_v3f(json_value_find(p, "point"), "plane.point",
                                    (V3f){0, 0, 0});
            plane.d = v3f_dot(plane.normal, plane.point);

            append_plane(scene, plane);
        }
    }

    const Json *titems = json_find(objects, "triangle");
    if (json_is_array(titems)) {
        size_t N = json_array_size(titems);
        for (size_t i = 0; i < N; i++) {
            parse_triangle(scene, json_array_at(titems, i));
        }
    }

    const Json *qitems = json_find(objects, "quad");
    if (json_is_array(qitems)) {
        size_t N = json_array_size(qitems);
        for (size_t i = 0; i < N; i++) {
            parse_quad(scene, json_array_at(qitems, i));
        }
    }

    const Json *bitems = json_find(objects, "boxes");
    if (json_is_array(bitems)) {
        size_t N = json_array_size(bitems);
        for (size_t i = 0; i < N; i++) {
            const Json_Value *b = json_array_at(bitems, i);

            int mi = parse_mat_index(json_value_find(b, "material"),
                                     scene->materials.size, "box.material");

            if (mi < 0) continue;

            V3f a = parse_v3f(json_value_find(b, "a"), "box.a", (V3f){0});
            V3f c = parse_v3f(json_value_find(b, "b"), "box.b", (V3f){0});

            add_box(scene, a, c, mi);
        }
    }

    const Json *mitems = json_find(objects, "models");
    if (json_is_array(mitems)) {
        size_t N = json_array_size(mitems);
        for (size_t i = 0; i < N; i++) {
            const Json_Value *m = json_array_at(mitems, i);

            const char *file_name =
                parse_string(json_value_find(m, "file"), "model.file");

            V3f position = parse_v3f(json_value_find(m, "position"),
                                     "model.position", (V3f){0});

            float scale =
                parse_float(json_value_find(m, "scale"), "model.scale", 1);

            add_model(scene, position, scale, file_name, &scene->materials);
        }
    }

END_PARSE:
    scene->bvh_root = construct_bvh(&scene->arena, scene->objects.items, 0,
                                    scene->objects.size);

    state->image =
        aligned_alloc(64, state->width * state->height * sizeof(uint32_t));
    if (!state->image) fatal("load_scene: image alloc failed: %s");

    scene->camera = camera;
    gettimeofday(&end, NULL);
    double ms = timersub_ms(&end, &start);

    Log(Log_Info, "load_scene: Loaded scene in %fms", ms);
}

void free_scene(Scene *scene) {
    if (!scene) return;

    arena_destroy(&scene->arena);

    vec_free(&scene->objects);
    vec_free(&scene->materials);
}
