#pragma once
#include "camera.h"
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
    int plane_count;
    int sphere_count;
    int material_count;
    Plane *planes;
    Sphere *spheres;
    Material *materials;
} Scene;

typedef struct {
    Scene scene;
    State state;
    Camera camera;
} JSON;

void print_summary(JSON res);
JSON load_scene(const char *scene_file);
