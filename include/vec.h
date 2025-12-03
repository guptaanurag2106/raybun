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
} V3f;

#define ORIGIN (V3f){0, 0, 0}

#define VECDEF static inline

VECDEF float v3f_slength(V3f vec) {
    return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
}

VECDEF float v3f_length(V3f vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

VECDEF bool v3f_near_zero(V3f vec) {
    return (vec.x < EPS) && (vec.y < EPS) && (vec.z < EPS);
}

VECDEF V3f v3f_normalize(V3f vec) {
    float len = v3f_length(vec);
    if (len == 0) return vec;
    return (V3f){.x = vec.x / len, .y = vec.y / len, .z = vec.z / len};
}

VECDEF V3f v3f_sub(V3f a, V3f b) {
    return (V3f){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

VECDEF V3f v3f_add(V3f a, V3f b) {
    return (V3f){.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
}

VECDEF float v3f_dot(V3f a, V3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

VECDEF V3f v3f_cross(V3f a, V3f b) {
    V3f r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

VECDEF V3f v3f_divf(V3f a, float b) {
    if (b == 0) return ORIGIN;
    return (V3f){a.x / b, a.y / b, a.z / b};
}

VECDEF V3f v3f_mulf(V3f a, float b) { return (V3f){a.x * b, a.y * b, a.z * b}; }

VECDEF V3f v3f_comp_mul(V3f a, V3f b) {
    return (V3f){a.x * b.x, a.y * b.y, a.z * b.z};
}

VECDEF V3f v3f_neg(V3f a) { return (V3f){-a.x, -a.y, -a.z}; }

VECDEF void v3f_print(V3f a) { printf("x: %f, y: %f, z; %f\n", a.x, a.y, a.z); }

VECDEF V3f v3f_clamp(V3f a, float min, float max) {
    return (V3f){clamp_float(a.x, min, max), clamp_float(a.y, min, max),
                 clamp_float(a.z, min, max)};
}

VECDEF V3f v3f_random() { return (V3f){randf(), randf(), randf()}; }

VECDEF V3f v3f_random_range(float min, float max) {
    return (V3f){rand_range(min, max), rand_range(min, max),
                 rand_range(min, max)};
}

VECDEF V3f
v3f_random_unit() {  // TODO: find better way for random vector on sphere
    while (true) {
        V3f a = v3f_random();
        float len = v3f_slength(a);
        if (len <= 1 && len >= 1e-50) {
            return v3f_divf(a, sqrtf(len));
        }
    }
}

VECDEF V3f v3f_random_in_unit_disk() {
    while (true) {
        V3f p = (V3f){rand_range(-1, 1), rand_range(-1, 1), 0};
        if (v3f_slength(p) < 1) return p;
    }
}

VECDEF V3f v3f_random_on_hemisphere(const V3f normal) {
    V3f on_unit_sphere = v3f_random_unit();
    if (v3f_dot(on_unit_sphere, normal) >
        0.0)  // In the same hemisphere as the normal
        return on_unit_sphere;
    else
        return v3f_neg(on_unit_sphere);
}

VECDEF V3f v3f_reflect(const V3f a, const V3f n) {
    return v3f_sub(a, v3f_mulf(n, 2 * v3f_dot(a, n)));
}

VECDEF V3f v3f_refract(const V3f uv, const V3f n, float etai_eta) {
    float cost = fmin(v3f_dot(v3f_neg(uv), n), 1);  // TODO: why fmin?

    V3f r_out_perp = v3f_mulf(v3f_add(uv, v3f_mulf(n, cost)), etai_eta);
    V3f r_out_parallel = v3f_mulf(n, -1 * sqrtf(1 - v3f_slength(r_out_perp)));

    return v3f_add(r_out_perp, r_out_parallel);
}

#ifdef _cplusplus
}
#endif

#endif  // VEC_H
