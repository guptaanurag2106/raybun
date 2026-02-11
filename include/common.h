#pragma once

#include "vec.h"

typedef V3f Colour;

typedef struct {
    float xmin, xmax;
    float ymin, ymax;
    float zmin, zmax;
} AABB;

typedef struct {
    V3f center;
    float radius;
    int mat_index;
} Sphere;

// FIX: no bounding box run separately
typedef struct {
    V3f normal;
    V3f point;  // any point on the plane
    float d;    // calculated internally normal.point
    int mat_index;
} Plane;

typedef struct {
    V3f v;
    V3f normal;
    V2f uv;
} Vertex;  // vertex for triangle

typedef struct {
    Vertex v1, v2, v3;
    V3f e1, e2;  // calculate internally edges
    int mat_index;
} Triangle;

typedef struct {
    V3f corner;
    V3f u;
    V3f v;
    V3f normal;  // calculate internally
    float d;     // calculate internally (Ax+By=Cz=D)
    V3f w;       // calculate internally n/(n.n) for normalized n its just n
    int mat_index;
} Quad;

typedef enum {
    MAT_NONE,  // TODO: MAT_NONE points to default texture?
    MAT_LAMBERTIAN,
    MAT_METAL,
    MAT_DIELECTRIC,
    MAT_EMISSIVE
} MaterialType;

static inline MaterialType string_to_mat(const char *s) {
    if (strcmp(s, "metal") == 0) return MAT_METAL;
    if (strcmp(s, "dielectric") == 0) return MAT_DIELECTRIC;
    if (strcmp(s, "lambertian") == 0) return MAT_LAMBERTIAN;
    if (strcmp(s, "emissive") == 0) return MAT_EMISSIVE;
    return MAT_NONE;
}

typedef enum { TEX_CONSTANT, TEX_IMAGE } TextureType;

typedef struct {
    TextureType type;
    union {
        Colour colour;
        // Image *image;
    };
} Texture;

typedef struct {
    MaterialType type;
    union {
        struct {
            Texture albedo;
            float fuzz;
        } metal;

        struct {
            Texture albedo;
        } lambertian;

        struct {
            float etai_eta;
        } dielectric;

        struct {
            Texture emission;
        } emissive;

    } properties;
} Material;

typedef struct {
    V3f origin;
    V3f direction;
    V3f inv_dir;
} Ray;

typedef struct {
    V3f point;
    V3f normal;
    float t;
    V2f uv;
    bool front_face;
    int mat_index;
} HitRecord;

typedef struct Hittable Hittable;

typedef bool (*Hitfn)(const Hittable *self, const Ray *r, float tmin,
                      float tmax, HitRecord *record);

// typedef bool (*AabbFn)(const Hittable *self, AABB *out_box);

typedef enum { HITTABLE_PRIMITIVE, HITTABLE_BVH } HittableType;

struct Hittable {
    Hitfn hit;
    AABB box;
    HittableType type;

    void *data;
};

typedef struct BVH_Node {
    Hittable left;
    Hittable right;
} BVH_Node;
