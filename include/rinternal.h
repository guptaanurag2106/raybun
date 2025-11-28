#pragma once

#include "vec.h"
#include "scene.h"

static inline v3f ray_at(Ray ray, float t) {
    return v3f_add(ray.origin, v3f_mulf(ray.direction, t));
}

bool sphere_hit(const Sphere *sphere, const Ray *r, float tmin, float tmax,
                HitRecord *record);

bool plane_hit(const Plane *plane, const Ray *r, float tmin, float tmax,
               HitRecord *record);

void set_face_normal(const Ray *r, const v3f *norm, HitRecord *record);

bool scene_hit(const Ray *r, Scene *scene, float tmin, float tmax,
               HitRecord *record);

static inline float linear_to_gamma(float linear_component) {
    if (linear_component > 0) return sqrtf(linear_component);

    return 0;
}

bool scatter(const Material *mat, const HitRecord *rec, const Ray *ray_in,
             Colour *attenuation, Ray *ray_out);
