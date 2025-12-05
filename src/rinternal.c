#include "rinternal.h"

#include "common.h"
#include "scene.h"
#include "vec.h"

inline void set_face_normal(const Ray *r, const V3f *norm, HitRecord *record) {
    record->front_face = v3f_dot(r->direction, *norm) < 0;
    record->normal = record->front_face ? *norm : v3f_neg(*norm);
}

bool sphere_hit(const Sphere *sphere, const Ray *ray, float tmin, float tmax,
                HitRecord *record) {
    V3f oc = v3f_sub(sphere->center, ray->origin);
    float a = v3f_slength(ray->direction);
    float h = v3f_dot(ray->direction, oc);
    float c = v3f_slength(oc) - sphere->radius * sphere->radius;

    float discriminant = (h * h - a * c);
    if (discriminant < 0) return false;

    float sqrtd = sqrtf(discriminant);
    float t = (h - sqrtd) / a;
    if (t <= tmin) {
        t = (h + sqrtd) / a;
        if (t <= tmin || t >= tmax) return false;
    } else if (t >= tmax)
        return false;

    record->t = t;
    record->point = ray_at(ray, t);
    record->mat_index = sphere->mat_index;
    V3f norm = v3f_divf(v3f_sub(record->point, sphere->center), sphere->radius);
    set_face_normal(ray, &norm, record);

    return true;
}

bool plane_hit(const Plane *plane, const Ray *ray, float tmin, float tmax,
               HitRecord *record) {
    const float nd = v3f_dot(plane->normal, ray->direction);
    if (fabs(nd) < EPS) return false;

    const float t = (plane->d - v3f_dot(plane->normal, ray->origin)) / nd;

    if (t <= tmin || t >= tmax) return false;

    record->t = t;
    record->point = ray_at(ray, t);
    record->mat_index = plane->mat_index;
    set_face_normal(ray, &plane->normal, record);

    return true;
}

bool triangle_hit(const Triangle *tr, const Ray *ray, float tmin, float tmax,
                  HitRecord *record) {
    V3f pvec = v3f_cross(ray->direction, tr->e2);
    float det = v3f_dot(tr->e1, pvec);

    if (det > -EPS && det < EPS) return false;

    float idet = 1.0 / det;
    V3f tvec = v3f_sub(ray->origin, tr->p1);
    float u = v3f_dot(tvec, pvec) * idet;
    if (u < 0 || u > 1) return false;

    V3f qvec = v3f_cross(tvec, tr->e1);
    float v = v3f_dot(ray->direction, qvec) * idet;
    if (v < 0 || (u + v) > 1) return false;

    float t = v3f_dot(tr->e2, qvec) * idet;
    if (t <= tmin || t >= tmax) return false;

    record->t = t;
    record->point = ray_at(ray, t);
    record->mat_index = tr->mat_index;
    set_face_normal(ray, &tr->normal, record);

    return true;
}

bool quad_hit(const Quad *quad, const Ray *ray, float tmin, float tmax,
              HitRecord *record) {
    const float nd = v3f_dot(quad->normal, ray->direction);
    if (fabs(nd) < EPS) return false;  // no parallel rays

    const float t = (quad->d - v3f_dot(quad->normal, ray->origin)) / nd;
    if (t <= tmin || t >= tmax) return false;

    V3f P = ray_at(ray, t);
    V3f p = v3f_sub(P, quad->corner);

    const float alpha = v3f_dot(v3f_cross(p, quad->v), quad->w);
    if (alpha > 1 || alpha < 0) return false;
    const float beta = v3f_dot(v3f_cross(quad->u, p), quad->w);
    if (beta > 1 || beta < 0) return false;

    record->t = t;
    record->point = P;
    record->mat_index = quad->mat_index;
    set_face_normal(ray, &quad->normal, record);

    return true;
}

bool scene_hit(const Ray *r, const Scene *scene, float tmin, float tmax,
               HitRecord *record) {
    HitRecord temp_rec;
    bool hit = false;
    for (int i = 0; i < scene->sphere_count; i++) {
        if (sphere_hit(&(scene->spheres[i]), r, tmin, tmax, &temp_rec)) {
            tmax = temp_rec.t;
            *record = temp_rec;
            hit = true;
        }
    }

    for (int i = 0; i < scene->plane_count; i++) {
        if (plane_hit(&(scene->planes[i]), r, tmin, tmax, &temp_rec)) {
            tmax = temp_rec.t;
            *record = temp_rec;
            hit = true;
        }
    }

    for (int i = 0; i < scene->triangle_count; i++) {
        if (triangle_hit(&(scene->triangles[i]), r, tmin, tmax, &temp_rec)) {
            tmax = temp_rec.t;
            *record = temp_rec;
            hit = true;
        }
    }

    for (int i = 0; i < scene->quad_count; i++) {
        if (quad_hit(&(scene->quads[i]), r, tmin, tmax, &temp_rec)) {
            tmax = temp_rec.t;
            *record = temp_rec;
            hit = true;
        }
    }
    return hit;
}
