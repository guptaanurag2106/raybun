#pragma once
#include "common.h"
#include "state.h"
#include "vec.h"

typedef struct {
    V3f position;
    V3f look_at;
    V3f up;
    float fov;           // radian
    float aspect_ratio;  // of viewport
    V3f forward;
    V3f right;
    float focal_length;  // 1 / tan(fov_y / 2)
    float defocus_angle;
    float focus_dist;

    float viewport_w;
    float viewport_h;
} Camera;

typedef struct {
    unsigned int scene_crc;
    char *scene_json;

    int plane_count;
    int sphere_count;
    int quad_count;
    int triangle_count;

    Hittable *objects;
    size_t obj_count;
    Hittable bvh_root;

    int material_count;
    Material *materials;

    Camera camera;
} Scene;

char *read_compress_scene(const char *scene_file);
void load_scene(const char *scene_file_content, Scene *scene, State *state);
void print_summary(const Scene *scene, const State *state);
void free_scene(Scene *scene);
