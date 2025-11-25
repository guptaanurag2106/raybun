#ifndef VEC_H
#define VEC_H

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _cplusplus
extern "C" {
#endif

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

VECDEF v3f v3f_divf(v3f a, float b) { return (v3f){a.x / b, a.y / b, a.z / b}; }

VECDEF v3f v3f_mulf(v3f a, float b) { return (v3f){a.x * b, a.y * b, a.z * b}; }

VECDEF void v3f_print(v3f a) { printf("x: %f, y: %f, z; %f\n", a.x, a.y, a.z); }

#ifdef _cplusplus
}
#endif

#endif  // VEC_H
