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
    int plane_count;
    int sphere_count;
    int quad_count;
    int material_count;
    Plane *planes;
    Sphere *spheres;
    Quad *quads;
    Material *materials;

    Camera camera;
} Scene;

typedef struct {
    Scene scene;  // world/scene, camera details (constants)
    State state;  // current image etc.
} JSON;

void print_summary(JSON res);
JSON load_scene(const char *scene_file);
