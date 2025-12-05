#include "rinternal.h"

#include "aabb.h"
#include "common.h"
#include "scene.h"
#include "vec.h"

static inline V3f ray_at(const Ray *ray, float t) {
    return v3f_add(ray->origin, v3f_mulf(ray->direction, t));
}

// FIX:marking it inline gives linker error with debug??
void set_face_normal(const Ray *r, const V3f *norm, HitRecord *record) {
    record->front_face = v3f_dot(r->direction, *norm) < 0;
    record->normal = record->front_face ? *norm : v3f_neg(*norm);
}

Hittable make_hittable_sphere(Sphere *s) {
    const float r = s->radius;
    AABB aabb = (AABB){
        .xmin = s->center.x - r,
        .xmax = s->center.x + r,
        .ymin = s->center.y - r,
        .ymax = s->center.y + r,
        .zmin = s->center.z - r,
        .zmax = s->center.z + r,
    };
    return (Hittable){
        .hit = sphere_hit, .type = HITTABLE_SPHERE, .data = s, .box = aabb};
}

Hittable make_hittable_plane(Plane *p) {
    // TODO: no bounding box
    return (Hittable){
        .hit = plane_hit, .type = HITTABLE_PLANE, .data = p, .box = {0}};
}

Hittable make_hittable_triangle(Triangle *t) {
    // same as quad
    AABB box1 = aabb(t->p1, v3f_add(t->p1, v3f_add(t->e1, t->e2)));
    AABB box2 = aabb(t->p2, t->p3);
    return (Hittable){.hit = triangle_hit,
                      .type = HITTABLE_TRIANGLE,
                      .data = t,
                      .box = aabb_join(box1, box2)};
}

Hittable make_hittable_quad(Quad *q) {
    AABB box1 = aabb(q->corner, v3f_add(q->corner, v3f_add(q->u, q->v)));
    AABB box2 = aabb(v3f_add(q->corner, q->u), v3f_add(q->corner, q->v));
    return (Hittable){.hit = quad_hit,
                      .type = HITTABLE_QUAD,
                      .data = q,
                      .box = aabb_join(box1, box2)};
}

Hittable make_hittable_bvh(BVH_Node *node, AABB box) {
    return (Hittable){
        .hit = aabb_hit, .type = HITTABLE_BVH, .data = node, .box = box};
}

inline bool sphere_hit(const Hittable *hittable, const Ray *ray, float tmin,
                       float tmax, HitRecord *record) {
    const Sphere *sphere = hittable->data;
    V3f oc = v3f_sub(sphere->center, ray->origin);
    float a = ray->dirslen;
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

inline bool plane_hit(const Hittable *hittable, const Ray *ray, float tmin,
                      float tmax, HitRecord *record) {
    const Plane *plane = hittable->data;
    const float nd = v3f_dot(plane->normal, ray->direction);
    if (nd > -EPS && nd < EPS) return false;

    const float t = (plane->d - v3f_dot(plane->normal, ray->origin)) / nd;

    if (t <= tmin || t >= tmax) return false;

    record->t = t;
    record->point = ray_at(ray, t);
    record->mat_index = plane->mat_index;
    set_face_normal(ray, &plane->normal, record);

    return true;
}

inline bool triangle_hit(const Hittable *hittable, const Ray *ray, float tmin,
                         float tmax, HitRecord *record) {
    const Triangle *tr = hittable->data;
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

inline bool quad_hit(const Hittable *hittable, const Ray *ray, float tmin,
                     float tmax, HitRecord *record) {
    const Quad *quad = hittable->data;
    const float nd = v3f_dot(quad->normal, ray->direction);
    if (nd > -EPS && nd < EPS) return false;  // no parallel rays

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

inline bool aabb_hit(const Hittable *h, const Ray *r, float tmin, float tmax,
                     HitRecord *rec) {
    const BVH_Node *node = h->data;
    const AABB box = h->box;
    const V3f *origin = &(r->origin);
    // x;
    float adinv = r->inv_dir.x;

    float t0 = (box.xmin - origin->x) * adinv;
    float t1 = (box.xmax - origin->x) * adinv;
    if (t0 < t1) {
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
    } else {
        if (t1 > tmin) tmin = t1;
        if (t0 < tmax) tmax = t0;
    }
    if (tmax <= tmin) return false;

    // y;
    adinv = r->inv_dir.y;
    t0 = (box.ymin - origin->y) * adinv;
    t1 = (box.ymax - origin->y) * adinv;
    if (t0 < t1) {
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
    } else {
        if (t1 > tmin) tmin = t1;
        if (t0 < tmax) tmax = t0;
    }
    if (tmax <= tmin) return false;

    // z;
    adinv = r->inv_dir.z;
    t0 = (box.zmin - origin->z) * adinv;
    t1 = (box.zmax - origin->z) * adinv;
    if (t0 < t1) {
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
    } else {
        if (t1 > tmin) tmin = t1;
        if (t0 < tmax) tmax = t0;
    }
    if (tmax <= tmin) return false;

    bool hit_left = node->left.hit(&node->left, r, tmin, tmax, rec);
    bool hit_right =
        node->right.hit(&node->right, r, tmin, hit_left ? rec->t : tmax, rec);

    return hit_left || hit_right;
}

inline bool scene_hit(const Ray *r, const Scene *scene, float tmin, float tmax,
                      HitRecord *record) {
    return scene->bvh_root.hit(&scene->bvh_root, r, tmin, tmax, record);
}
