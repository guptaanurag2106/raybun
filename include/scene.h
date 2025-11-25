#pragma once
#include "vec.h"
#include "state.h"


typedef struct {
    v3f center;
    float radius;
} Sphere;

typedef struct {
    v3f nomral;
} Plane;

typedef struct {
    int plane_count;
    int sphere_count;
    Plane *planes;
    Sphere *spheres;
} Scene;

typedef struct {
    Scene scene;
    State state;
}JSON;

JSON load_scene(const char *scene_file) ;
