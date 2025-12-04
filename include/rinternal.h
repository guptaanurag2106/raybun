#pragma once

#include "scene.h"
#include "vec.h"

static inline V3f ray_at(Ray ray, float t) {
    return v3f_add(ray.origin, v3f_mulf(ray.direction, t));
}

bool scene_hit(const Ray *r, Scene *scene, float tmin, float tmax,
               HitRecord *record);

static inline float linear_to_gamma(float linear_component) {
    if (linear_component > 0) return sqrtf(linear_component);
    return 0;
}

bool scatter(const Material *mat, const HitRecord *rec, const Ray *ray_in,
             Colour *attenuation, Ray *ray_out);
