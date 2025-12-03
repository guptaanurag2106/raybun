#pragma once

#include "vec.h"

typedef V3f Colour;

typedef struct {
    V3f center;
    float radius;
    int mat_index;
} Sphere;

typedef struct {
    V3f normal;
    V3f point;  // any point on the plane
    float d;    // calculated internally normal.point
    int mat_index;
} Plane;

typedef struct {
    V3f corner;
    V3f u;
    V3f v;
    V3f normal;  // calculate internally
    float d;     // calculate internally (Ax+By=Cz=D)
    V3f w;       // calculate internally n/(n.n) for normalized n its just n
    int mat_index;
} Quad;

typedef struct {
} Triangle;

typedef enum {
    MAT_NONE,
    MAT_LAMBERTIAN,
    MAT_METAL,
    MAT_DIELECTRIC,
    MAT_EMISSIVE
} MaterialType;

static inline MaterialType mat_to_string(const char *s) {
    if (strcmp(s, "metal") == 0) return MAT_METAL;
    if (strcmp(s, "dielectric") == 0) return MAT_DIELECTRIC;
    if (strcmp(s, "lambertian") == 0) return MAT_LAMBERTIAN;
    if (strcmp(s, "emissive") == 0) return MAT_EMISSIVE;
    return MAT_NONE;
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

        struct {
            Colour emission;
        } emissive;

    } properties;
} Material;

typedef struct {
    V3f origin;
    V3f direction;
} Ray;

typedef struct {
    V3f point;
    V3f normal;
    float t;
    bool front_face;
    int mat_index;
} HitRecord;
