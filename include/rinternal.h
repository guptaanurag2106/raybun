#pragma once

#include "common.h"
#include "scene.h"

Hittable make_hittable_sphere(Sphere *s);
Hittable make_hittable_plane(Plane *p);
Hittable make_hittable_triangle(Triangle *t);
Hittable make_hittable_quad(Quad *q);
Hittable make_hittable_bvh(BVH_Node *node, AABB box);

bool sphere_hit(const Hittable *hittable, const Ray *ray, float tmin,
                float tmax, HitRecord *record);

bool plane_hit(const Hittable *hittable, const Ray *ray, float tmin, float tmax,
               HitRecord *record);

bool triangle_hit(const Hittable *hittable, const Ray *ray, float tmin,
                  float tmax, HitRecord *record);

bool quad_hit(const Hittable *hittable, const Ray *ray, float tmin, float tmax,
              HitRecord *record);

bool scene_hit(const Ray *r, const Scene *scene, float tmin, float tmax,
               HitRecord *record);

bool aabb_hit(const Hittable *h, const Ray *r, float tmin, float tmax,
              HitRecord *rec);

bool scatter(const Material *mat, const HitRecord *rec, const Ray *ray_in,
             Colour *attenuation, Ray *ray_out);
