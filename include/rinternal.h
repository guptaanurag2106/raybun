#pragma once

#include "common.h"
#include "scene.h"

Hittable make_hittable_sphere(Sphere *s);
Hittable make_hittable_plane(Plane *p);
Hittable make_hittable_triangle(Triangle *t);
Hittable make_hittable_quad(Quad *q);
Hittable make_hittable_bvh(BVH_Node *node, AABB box);

bool scene_hit(const Ray *r, const Scene *scene, float tmin, float tmax,
               HitRecord *record);

bool scatter(const Material *mat, const HitRecord *rec, const Ray *ray_in,
             Colour *attenuation, Ray *ray_out);
