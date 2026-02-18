#ifndef VEC_H_
#define VEC_H_

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "utils.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef VECDEF
#define VECDEF static inline
#endif

#define EPS 1e-8f

typedef struct {
    float x, y;
} V2f;

VECDEF V2f v2f_add(V2f a, V2f b) { return (V2f){a.x + b.x, a.y + b.y}; }

VECDEF V2f v2f_sub(V2f a, V2f b) { return (V2f){a.x - b.x, a.y - b.y}; }

VECDEF V2f v2f_mulf(V2f a, float s) { return (V2f){a.x * s, a.y * s}; }

VECDEF V2f v2f_divf(V2f a, float s) {
    if (s == 0) return (V2f){0, 0};
    return (V2f){a.x / s, a.y / s};
}

VECDEF float v2f_dot(V2f a, V2f b) { return a.x * b.x + a.y * b.y; }

VECDEF float v2f_length_sq(V2f a) { return a.x * a.x + a.y * a.y; }

VECDEF float v2f_length(V2f a) { return sqrtf(v2f_length_sq(a)); }

VECDEF V2f v2f_normalize(V2f a) {
    float len = v2f_length(a);
    if (len == 0) return a;
    return v2f_divf(a, len);
}

VECDEF bool v2f_near_zero(V2f a) {
    return (fabsf(a.x) < EPS) && (fabsf(a.y) < EPS);
}

VECDEF void v2f_print(V2f a) { printf("x: %f, y: %f\n", a.x, a.y); }

typedef struct {
    float x, y, z;
} V3f;

#define ORIGIN (V3f){0, 0, 0}

VECDEF float v3f_slength(V3f a) { return a.x * a.x + a.y * a.y + a.z * a.z; }

VECDEF float v3f_length(V3f a) {
    return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

VECDEF bool v3f_near_zero(V3f a) {
    return (a.x < EPS) && (a.y < EPS) && (a.z < EPS);
}

VECDEF V3f v3f_normalize(V3f a) {
    float len = v3f_length(a);
    if (len == 0) return a;
    return (V3f){.x = a.x / len, .y = a.y / len, .z = a.z / len};
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

VECDEF V3f v3f_inv(V3f a) { return (V3f){1 / a.x, 1 / a.y, 1 / a.z}; }

VECDEF void v3f_print(V3f a) { printf("x: %f, y: %f, z; %f\n", a.x, a.y, a.z); }

VECDEF V3f v3f_clamp(V3f a, float min, float max) {
    return (V3f){clamp_float(a.x, min, max), clamp_float(a.y, min, max),
                 clamp_float(a.z, min, max)};
}

VECDEF V3f v3f_random() {
    return (V3f){rng_f32_tls(), rng_f32_tls(), rng_f32_tls()};
}

VECDEF V3f v3f_random_range(float min, float max) {
    return (V3f){rngf_range_tls(min, max), rngf_range_tls(min, max),
                 rngf_range_tls(min, max)};
}

// TODO: find better way for random vector on sphere
VECDEF V3f v3f_random_unit() {
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
        V3f p = (V3f){rngf_range_tls(-1, 1), rngf_range_tls(-1, 1), 0};
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

#ifdef __cplusplus
}
#endif
#endif  // VEC_H_
