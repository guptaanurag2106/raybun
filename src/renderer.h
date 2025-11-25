#pragma once

#include "scene.h"
#include "vec.h"

typedef struct {
    v3f origin;
    v3f direction;
} Ray;

typedef struct {
    int sphere_index;
    float t;
} Hit;

static inline v3f ray_at(Ray ray, float t) {
    return v3f_add(ray.origin, v3f_mulf(ray.direction, t));
}

void calculate_camera_fields(Camera *cam);
void render_scene(Scene *scene, State *state);
