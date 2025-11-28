#ifndef VEC_H
#define VEC_H

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "utils.h"
#ifdef _cplusplus
extern "C" {
#endif

#define EPS 1e-8

typedef struct {
    float x, y, z;
} v3f;

#define ORIGIN (v3f){0, 0, 0}

#define VECDEF static inline

VECDEF float v3f_slength(v3f vec) {
    return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
}

VECDEF float v3f_length(v3f vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

VECDEF bool v3f_near_zero(v3f vec) {
    return (vec.x < EPS) && (vec.y < EPS) && (vec.z < EPS);
}

VECDEF v3f v3f_normalize(v3f vec) {
    float len = v3f_length(vec);
    if (len == 0) return vec;
    return (v3f){.x = vec.x / len, .y = vec.y / len, .z = vec.z / len};
}

VECDEF v3f v3f_sub(v3f a, v3f b) {
    return (v3f){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

VECDEF v3f v3f_add(v3f a, v3f b) {
    return (v3f){.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
}

VECDEF float v3f_dot(v3f a, v3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

VECDEF v3f v3f_cross(v3f a, v3f b) {
    v3f r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

VECDEF v3f v3f_divf(v3f a, float b) {
    if (b == 0) return ORIGIN;
    return (v3f){a.x / b, a.y / b, a.z / b};
}

VECDEF v3f v3f_mulf(v3f a, float b) { return (v3f){a.x * b, a.y * b, a.z * b}; }

VECDEF v3f v3f_comp_mul(v3f a, v3f b) {
    return (v3f){a.x * b.x, a.y * b.y, a.z * b.z};
}

VECDEF v3f v3f_neg(v3f a) { return (v3f){-a.x, -a.y, -a.z}; }

VECDEF void v3f_print(v3f a) { printf("x: %f, y: %f, z; %f\n", a.x, a.y, a.z); }

VECDEF v3f v3f_clamp(v3f a, float min, float max) {
    return (v3f){clamp_float(a.x, min, max), clamp_float(a.y, min, max),
                 clamp_float(a.z, min, max)};
}

VECDEF v3f v3f_random() { return (v3f){randf(), randf(), randf()}; }

VECDEF v3f v3f_random_range(float min, float max) {
    return (v3f){rand_range(min, max), rand_range(min, max),
                 rand_range(min, max)};
}

VECDEF v3f
v3f_random_unit() {  // TODO: find better way for random vector on sphere
    while (true) {
        v3f a = v3f_random();
        float len = v3f_slength(a);
        if (len <= 1 && len >= 1e-50) {
            return v3f_divf(a, sqrtf(len));
        }
    }
}

VECDEF v3f v3f_random_in_unit_disk() {
    while (true) {
        v3f p = (v3f){rand_range(-1, 1), rand_range(-1, 1), 0};
        if (v3f_slength(p) < 1) return p;
    }
}

VECDEF v3f v3f_random_on_hemisphere(const v3f normal) {
    v3f on_unit_sphere = v3f_random_unit();
    if (v3f_dot(on_unit_sphere, normal) >
        0.0)  // In the same hemisphere as the normal
        return on_unit_sphere;
    else
        return v3f_neg(on_unit_sphere);
}

VECDEF v3f v3f_reflect(const v3f a, const v3f n) {
    return v3f_sub(a, v3f_mulf(n, 2 * v3f_dot(a, n)));
}

VECDEF v3f v3f_refract(const v3f uv, const v3f n, float etai_eta) {
    float cost = fmin(v3f_dot(v3f_neg(uv), n), 1);  // TODO: why fmin?

    v3f r_out_perp = v3f_mulf(v3f_add(uv, v3f_mulf(n, cost)), etai_eta);
    v3f r_out_parallel = v3f_mulf(n, -1 * sqrtf(1 - v3f_slength(r_out_perp)));

    return v3f_add(r_out_perp, r_out_parallel);
}

#ifdef _cplusplus
}
#endif

#endif  // VEC_H
