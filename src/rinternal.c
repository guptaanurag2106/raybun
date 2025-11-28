#include "rinternal.h"

#include "scene.h"
#include "vec.h"

void set_face_normal(const Ray *r, const v3f *norm, HitRecord *record) {
    record->front_face = v3f_dot(r->direction, *norm) < 0;
    record->normal = record->front_face ? *norm : v3f_neg(*norm);
}

bool sphere_hit(const Sphere *sphere, const Ray *ray, float tmin, float tmax,
                HitRecord *record) {
    v3f oc = v3f_sub(sphere->center, ray->origin);
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
    record->point = ray_at(*ray, t);
    record->mat_index = sphere->mat_index;
    v3f norm = v3f_divf(v3f_sub(record->point, sphere->center), sphere->radius);
    set_face_normal(ray, &norm, record);

    return true;
}

bool plane_hit(const Plane *plane, const Ray *ray, float tmin, float tmax,
               HitRecord *record) {
    float nr = v3f_dot(plane->normal, ray->origin);
    float np = v3f_dot(plane->normal, plane->point);
    float nd = v3f_dot(plane->normal, ray->direction);

    float t = (np - nr) / nd;

    if (t <= tmin || t >= tmax) return false;

    record->t = t;
    record->point = ray_at(*ray, t);
    record->mat_index = plane->mat_index;
    set_face_normal(ray, &plane->normal, record);

    return true;
}

bool scene_hit(const Ray *r, Scene *scene, float tmin, float tmax,
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

    return hit;
}
