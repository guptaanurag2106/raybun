#pragma once
#include "state.h"
#include "vec.h"

typedef struct {
    v3f center;
    float radius;
} Sphere;

typedef struct {
    v3f normal;
} Plane;

typedef struct {
} Material;

typedef struct {
    v3f position;
    v3f look_at;
    v3f up;
    v3f forward;
    v3f right;
    float fov;           // radian
    float focal_length;  // 1 / tan(fov_y / 2)
    float aspect_ratio;  // of viewport

    float viewport_w;
    float viewport_h;
} Camera;

typedef struct {
    int plane_count;
    int sphere_count;
    int material_count;
    Plane *planes;
    Sphere *spheres;
    Material *materials;

    Camera camera;
} Scene;

typedef struct {
    Scene scene;  // world/scene, camera details (constants)
    State state;  // current image etc.
} JSON;

void print_summary(JSON res);
JSON load_scene(const char *scene_file);
