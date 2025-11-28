#pragma once

#include "vec.h"
typedef v3f Colour;
typedef struct {
    v3f center;
    float radius;
    int mat_index;
} Sphere;

typedef struct {
    v3f normal;
    v3f point;  // any point on the plane
    int mat_index;
} Plane;

typedef enum { MAT_LAMBERTIAN, MAT_METAL, MAT_DIELECTRIC } MaterialType;

static inline MaterialType mat_to_string(const char *s) {
    if (strcmp(s, "metal") == 0) return MAT_METAL;
    if (strcmp(s, "dielectric") == 0) return MAT_DIELECTRIC;
    if (strcmp(s, "lambertian") == 0) return MAT_LAMBERTIAN;
    return MAT_LAMBERTIAN;
}

typedef struct {
    MaterialType type;
    union {
        struct {
            Colour albedo;
            float fuzz;
        } metal;

        struct {
            Colour albedo;
        } lambertian;

        struct {
            float etai_eta;
        } dielectric;

    } properties;
    const char *name;
} Material;

typedef struct {
    v3f origin;
    v3f direction;
} Ray;

typedef struct {
    v3f point;
    v3f normal;
    float t;
    bool front_face;
    int mat_index;
} HitRecord;
