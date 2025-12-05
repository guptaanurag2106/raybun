#include <cJSON.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "aabb.h"
#include "common.h"
#include "scene.h"
#include "utils.h"
#include "vec.h"

void fatal(const char *msg) {
    Log(Log_Error, msg);
    exit(1);
}

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
    Log(Log_Info, temp_sprintf("load_scene: Loaded %d triangles",
                               res.scene.triangle_count));
    Log(Log_Info, temp_sprintf("load_scene: Loaded %d materials",
                               res.scene.material_count));
}

// TODO: check what all actually needs to be normalized
static V3f parse_v3f(cJSON *arr, const char *ctx, V3f fallback) {
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) != 3) {
        log_warn(temp_sprintf("%s: expected array[3], using default.", ctx));
        printf("asdf: %s  %d", ctx, cJSON_GetArraySize(arr));
        return fallback;
    }
    return (V3f){cJSON_GetArrayItem(arr, 0)->valuedouble,
                 cJSON_GetArrayItem(arr, 1)->valuedouble,
                 cJSON_GetArrayItem(arr, 2)->valuedouble};
}

static float parse_float(cJSON *node, const char *ctx, float fallback) {
    if (!cJSON_IsNumber(node)) {
        log_warn(temp_sprintf("%s: expected number, using default.", ctx));
        return fallback;
    }
    return node->valuedouble;
}

static int parse_int(cJSON *node, const char *ctx, int fallback) {
    if (!cJSON_IsNumber(node)) {
        log_warn(temp_sprintf("%s: expected integer, using default.", ctx));
        return fallback;
    }
    return node->valueint;
}

static int parse_mat_index(cJSON *node, int mat_count, const char *ctx) {
    if (!cJSON_IsNumber(node) || node->valueint < 0 ||
        node->valueint >= mat_count) {
        log_warn(temp_sprintf("%s: invalid material index.", ctx));
        return -1;
    }
    return node->valueint;
}

static Quad make_quad(V3f corner, V3f u, V3f v, int mat_index) {
    Quad q = {0};
    q.corner = corner;
    q.u = u;
    q.v = v;
    q.mat_index = mat_index;

    V3f n = v3f_cross(u, v);
    float L = v3f_slength(n);
    V3f nn = v3f_mulf(n, 1.0f / sqrtf(L));

    q.normal = nn;
    q.d = v3f_dot(nn, corner);
    q.w = v3f_mulf(n, 1.0f / L);
    q.aabb = aabb(q.corner, v3f_add(q.corner, v3f_add(q.u, q.v)));
    return q;
}

static Triangle make_triangle(V3f p1, V3f p2, V3f p3, int mat_index) {
    V3f e1 = v3f_sub(p2, p1);
    V3f e2 = v3f_sub(p3, p1);
    return (Triangle){.p1 = p1,
                      .p2 = p2,
                      .p3 = p3,
                      .normal = v3f_cross(e1, e2),
                      .e1 = e1,
                      .e2 = e2,
                      .mat_index = mat_index};
}

static void append_quad(Scene *scene, Quad quad) {
    scene->quads =
        realloc(scene->quads, sizeof(Quad) * (scene->quad_count + 1));
    scene->quads[scene->quad_count++] = quad;
}

static void append_triangle(Scene *scene, Triangle triangle) {
    scene->triangles = realloc(scene->triangles,
                               sizeof(Triangle) * (scene->triangle_count + 1));
    scene->triangles[scene->triangle_count++] = triangle;
}

static void add_box(Scene *scene, V3f a, V3f b, int mat_index) {
    V3f minp = {MIN(a.x, b.x), MIN(a.y, b.y), MIN(a.z, b.z)};

    V3f maxp = {MAX(a.x, b.x), MAX(a.y, b.y), MAX(a.z, b.z)};

    V3f dx = {maxp.x - minp.x, 0, 0};
    V3f dy = {0, maxp.y - minp.y, 0};
    V3f dz = {0, 0, maxp.z - minp.z};

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

static int parse_quad(Scene *scene, cJSON *qnode) {
    cJSON *corner = cJSON_GetObjectItemCaseSensitive(qnode, "corner");
    cJSON *u = cJSON_GetObjectItemCaseSensitive(qnode, "u");
    cJSON *v = cJSON_GetObjectItemCaseSensitive(qnode, "v");
    cJSON *mat_i = cJSON_GetObjectItemCaseSensitive(qnode, "material");

    int mi = parse_mat_index(mat_i, scene->material_count, "quad.material");
    if (mi < 0) return 0;

    V3f C = parse_v3f(corner, "quad.corner", (V3f){0});
    V3f U = parse_v3f(u, "quad.u", (V3f){0});
    V3f V = parse_v3f(v, "quad.v", (V3f){0});

    append_quad(scene, make_quad(C, U, V, mi));
    return 1;
}

static int parse_triangle(Scene *scene, cJSON *tnode) {
    cJSON *p1 = cJSON_GetObjectItemCaseSensitive(tnode, "p1");
    cJSON *p2 = cJSON_GetObjectItemCaseSensitive(tnode, "p2");
    cJSON *p3 = cJSON_GetObjectItemCaseSensitive(tnode, "p3");
    cJSON *mat_i = cJSON_GetObjectItemCaseSensitive(tnode, "material");

    int mi = parse_mat_index(mat_i, scene->material_count, "triangle.material");
    if (mi < 0) return 0;

    V3f P1 = parse_v3f(p1, "triangle.p1", (V3f){0});
    V3f P2 = parse_v3f(p2, "triangle.p2", (V3f){0});
    V3f P3 = parse_v3f(p3, "triangle.p3", (V3f){0});

    append_triangle(scene, make_triangle(P1, P2, P3, mi));
    return 1;
}

JSON load_scene(const char *scene_file) {
    const char *file = read_entire_file(scene_file);
    if (!file) fatal("load_scene: cannot read file.");

    cJSON *json = cJSON_Parse(file);
    free((void *)file);

    if (!json) {
        fatal(temp_sprintf(
            "load_scene: JSON parse error near: %s",
            cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown"));
    }

    Scene scene = {0};
    State state = {
        .width = 1024, .height = 768, .samples_per_pixel = 10, .max_depth = 10};
    Camera camera = {.position = {0, 0, -5},
                     .look_at = {0, 0, 0},
                     .up = {0, 1, 0},
                     .fov = DEG2RAD(60),
                     .aspect_ratio = 4.f / 3.f,
                     .defocus_angle = 0,
                     .focus_dist = 1};

    cJSON *config = cJSON_GetObjectItemCaseSensitive(json, "config");
    if (cJSON_IsObject(config)) {
        state.width =
            parse_int(cJSON_GetObjectItemCaseSensitive(config, "width"),
                      "config.width", state.width);
        state.height =
            parse_int(cJSON_GetObjectItemCaseSensitive(config, "height"),
                      "config.height", state.height);
        state.samples_per_pixel = parse_int(
            cJSON_GetObjectItemCaseSensitive(config, "samples_per_pixel"),
            "config.spp", state.samples_per_pixel);
        state.max_depth =
            parse_int(cJSON_GetObjectItemCaseSensitive(config, "max_depth"),
                      "config.max_depth", state.max_depth);
    } else
        log_warn("config: not found, using defaults.");

    cJSON *cam = cJSON_GetObjectItemCaseSensitive(json, "camera");
    if (cJSON_IsObject(cam)) {
        camera.fov =
            DEG2RAD(parse_float(cJSON_GetObjectItemCaseSensitive(cam, "fov"),
                                "camera.fov", RAD2DEG(camera.fov)));
        camera.defocus_angle = DEG2RAD(
            parse_float(cJSON_GetObjectItemCaseSensitive(cam, "defocus_angle"),
                        "camera.defocus_angle", 0));
        camera.focus_dist =
            parse_float(cJSON_GetObjectItemCaseSensitive(cam, "focus_dist"),
                        "camera.focus_dist", camera.focus_dist);

        // aspect ratio (fraction format)
        cJSON *ar = cJSON_GetObjectItemCaseSensitive(cam, "aspect_ratio");
        if (cJSON_IsString(ar)) {
            int n, d;
            if (sscanf(ar->valuestring, "%d/%d", &n, &d) == 2 && d != 0)
                camera.aspect_ratio = (float)n / d;
            else
                log_warn(
                    "camera.aspect_ratio: invalid fraction, using default.");
        }

        camera.position =
            parse_v3f(cJSON_GetObjectItemCaseSensitive(cam, "position"),
                      "camera.position", camera.position);
        camera.look_at = v3f_normalize(
            parse_v3f(cJSON_GetObjectItemCaseSensitive(cam, "look_at"),
                      "camera.look_at", camera.look_at));
        camera.up =
            v3f_normalize(parse_v3f(cJSON_GetObjectItemCaseSensitive(cam, "up"),
                                    "camera.up", camera.up));
    } else
        log_warn("camera: not found, using defaults.");

    cJSON *materials = cJSON_GetObjectItemCaseSensitive(json, "materials");
    if (cJSON_IsArray(materials)) {
        int N = cJSON_GetArraySize(materials);
        scene.materials = malloc(sizeof(Material) * N);

        int M = 0;
        for (int i = 0; i < N; i++) {
            cJSON *mt = cJSON_GetArrayItem(materials, i);
            cJSON *type = cJSON_GetObjectItemCaseSensitive(mt, "type");
            if (!cJSON_IsString(type)) {
                log_warn("material: missing/invalid 'type', skipping.");
                continue;
            }

            Material *dst = &scene.materials[M];
            dst->type = mat_to_string(type->valuestring);

            cJSON *albedo = cJSON_GetObjectItemCaseSensitive(mt, "albedo");
            cJSON *emission = cJSON_GetObjectItemCaseSensitive(mt, "emission");

            switch (dst->type) {
                case MAT_LAMBERTIAN:
                    dst->properties.lambertian.albedo =
                        parse_v3f(albedo, "material.albedo", (V3f){1, 1, 1});
                    break;

                case MAT_METAL: {
                    dst->properties.metal.albedo =
                        parse_v3f(albedo, "material.albedo", (V3f){1, 1, 1});
                    float f = parse_float(
                        cJSON_GetObjectItemCaseSensitive(mt, "fuzz"),
                        "material.fuzz", 0);
                    dst->properties.metal.fuzz = clamp_float(f, 0, 1);
                } break;

                case MAT_EMISSIVE:
                    dst->properties.emissive.emission = parse_v3f(
                        emission, "material.emission", (V3f){0, 0, 0});
                    break;

                case MAT_DIELECTRIC:
                    dst->properties.dielectric.etai_eta =
                        parse_float(cJSON_GetObjectItemCaseSensitive(
                                        mt, "refraction_index"),
                                    "material.refraction_index", 1);
                    break;

                default:
                    log_warn("material: unknown type, skipping.");
                    continue;
            }

            M++;
        }
        scene.material_count = M;
        scene.materials = realloc(scene.materials, sizeof(Material) * M);
    }

    cJSON *objects = cJSON_GetObjectItemCaseSensitive(json, "objects");
    if (!cJSON_IsObject(objects)) {
        log_warn("objects: section missing/malformed.");
        goto END_PARSE;
    }

    cJSON *sitems = cJSON_GetObjectItemCaseSensitive(objects, "sphere");
    if (cJSON_IsArray(sitems)) {
        int N = cJSON_GetArraySize(sitems);
        scene.spheres = malloc(sizeof(Sphere) * N);
        int S = 0;

        for (int i = 0; i < N; i++) {
            cJSON *s = cJSON_GetArrayItem(sitems, i);
            int mi =
                parse_mat_index(cJSON_GetObjectItemCaseSensitive(s, "material"),
                                scene.material_count, "sphere.material");
            if (mi < 0) continue;

            cJSON *center = cJSON_GetObjectItemCaseSensitive(s, "center");
            cJSON *radius = cJSON_GetObjectItemCaseSensitive(s, "radius");

            Sphere *dst = &scene.spheres[S];
            dst->mat_index = mi;
            dst->center = parse_v3f(center, "sphere.center", (V3f){0});
            const float r = parse_float(radius, "sphere.radius", 0);
            dst->radius = r;
            dst->aabb = (AABB){
                .xmin = dst->center.x - r,
                .xmax = dst->center.x + r,
                .ymin = dst->center.y - r,
                .ymax = dst->center.y + r,
                .zmin = dst->center.z - r,
                .zmax = dst->center.z + r,
            };
            if (dst->radius <= 0) {
                log_warn("sphere.radius: must be >0, skipping.");
                continue;
            }
            S++;
        }

        scene.sphere_count = S;
        scene.spheres = realloc(scene.spheres, sizeof(Sphere) * S);
    }

    cJSON *pitems = cJSON_GetObjectItemCaseSensitive(objects, "plane");
    if (cJSON_IsArray(pitems)) {
        int N = cJSON_GetArraySize(pitems);
        scene.planes = malloc(sizeof(Plane) * N);
        int P = 0;

        for (int i = 0; i < N; i++) {
            cJSON *p = cJSON_GetArrayItem(pitems, i);
            int mi =
                parse_mat_index(cJSON_GetObjectItemCaseSensitive(p, "material"),
                                scene.material_count, "plane.material");
            if (mi < 0) continue;

            Plane *dst = &scene.planes[P];
            dst->mat_index = mi;
            dst->normal = v3f_normalize(
                parse_v3f(cJSON_GetObjectItemCaseSensitive(p, "normal"),
                          "plane.normal", (V3f){0, 1, 0}));
            dst->point = parse_v3f(cJSON_GetObjectItemCaseSensitive(p, "point"),
                                   "plane.point", (V3f){0, 0, 0});
            dst->d = v3f_dot(dst->normal, dst->point);
            P++;
        }

        scene.plane_count = P;
        scene.planes = realloc(scene.planes, sizeof(Plane) * P);
    }

    cJSON *qitems = cJSON_GetObjectItemCaseSensitive(objects, "quad");
    if (cJSON_IsArray(qitems)) {
        int N = cJSON_GetArraySize(qitems);
        for (int i = 0; i < N; i++) {
            parse_quad(&scene, cJSON_GetArrayItem(qitems, i));
        }
    }

    cJSON *titems = cJSON_GetObjectItemCaseSensitive(objects, "triangle");
    if (cJSON_IsArray(titems)) {
        int N = cJSON_GetArraySize(titems);
        for (int i = 0; i < N; i++) {
            parse_triangle(&scene, cJSON_GetArrayItem(titems, i));
        }
    }

    cJSON *bitems = cJSON_GetObjectItemCaseSensitive(objects, "boxes");
    if (cJSON_IsArray(bitems)) {
        int N = cJSON_GetArraySize(bitems);
        for (int i = 0; i < N; i++) {
            cJSON *b = cJSON_GetArrayItem(bitems, i);

            int mi =
                parse_mat_index(cJSON_GetObjectItemCaseSensitive(b, "material"),
                                scene.material_count, "box.material");

            if (mi < 0) continue;

            V3f a = parse_v3f(cJSON_GetObjectItemCaseSensitive(b, "a"), "box.a",
                              (V3f){0});
            V3f c = parse_v3f(cJSON_GetObjectItemCaseSensitive(b, "b"), "box.b",
                              (V3f){0});

            add_box(&scene, a, c, mi);
        }
    }

END_PARSE:
    cJSON_Delete(json);

    state.image = malloc(state.width * state.height * sizeof(uint32_t));
    if (!state.image)
        fatal(temp_sprintf("load_scene: image alloc failed: %s",
                           strerror(errno)));

    scene.camera = camera;

    Log(Log_Info, temp_sprintf("load_scene: Loaded %s", scene_file));

    return (JSON){.scene = scene, .state = state};
}
