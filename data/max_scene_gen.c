#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    float x, y, z, r;
} Bound;

/********************************************************
 * Global for overlap checking
 ********************************************************/
#define MAX_OBJECTS 1000
static Bound objs[MAX_OBJECTS];
static int obj_count = 0;

static inline float frand(float a, float b) {
    return a + (b - a) * ((float)rand() / RAND_MAX);
}

static bool overlaps(float x, float y, float z, float r) {
    for (int i = 0; i < obj_count; i++) {
        float dx = x - objs[i].x;
        float dy = y - objs[i].y;
        float dz = z - objs[i].z;
        float d2 = dx * dx + dy * dy + dz * dz;
        float rr = r + objs[i].r;
        if (d2 < rr * rr) return true;
    }
    return false;
}

static void add_bound(float x, float y, float z, float r) {
    objs[obj_count++] = (Bound){x, y, z, r};
}

/********************************************************
 * Spheres
 ********************************************************/
void gen_spheres(int count) {
    printf("    \"sphere\": [\n");

    for (int i = 0; i < count; i++) {
        float cx, cy, cz, r;
        int tries = 0;

        do {
            cx = frand(-60, 60);
            cy = frand(-30, 30);
            cz = frand(-60, 60);
            r = frand(1.0f, 2.0f);
            tries++;
        } while (overlaps(cx, cy, cz, r) && tries < 800);

        add_bound(cx, cy, cz, r);
        int mat = rand() % 7;  // 0–6 inclusive

        printf(
            "      { \"center\": [%.3f, %.3f, %.3f], \"radius\": %.3f, "
            "\"material\": %d }%s\n",
            cx, cy, cz, r, mat, (i < count - 1 ? "," : ""));
    }

    printf("    ],\n");
}

/********************************************************
 * Triangles
 ********************************************************/
void gen_triangles(int count) {
    printf("    \"triangle\": [\n");

    for (int i = 0; i < count; i++) {
        float bx, by, bz;
        float r = 3.0f;
        int tries = 0;

        do {
            bx = frand(-50, 50);
            by = frand(-30, 40);
            bz = frand(-50, 50);
            tries++;
        } while (overlaps(bx, by, bz, r) && tries < 800);

        add_bound(bx, by, bz, r);

        float p1x = bx, p1y = by, p1z = bz;
        float p2x = bx + frand(-4, 4), p2y = by + frand(0, 8),
              p2z = bz + frand(-4, 4);
        float p3x = bx + frand(-4, 4), p3y = by + frand(-8, 0),
              p3z = bz + frand(-4, 4);

        int mat = rand() % 7;

        printf(
            "      { \"p1\": [%.3f,%.3f,%.3f], \"p2\": [%.3f,%.3f,%.3f], "
            "\"p3\": [%.3f,%.3f,%.3f], \"material\": %d }%s\n",
            p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, mat,
            (i < count - 1 ? "," : ""));
    }

    printf("    ],\n");
}

/********************************************************
 * Quads
 ********************************************************/
void gen_quads(int count) {
    printf("    \"quad\": [\n");

    for (int i = 0; i < count; i++) {
        float cx, cy, cz;
        float r = 4.0f;
        int tries = 0;

        do {
            cx = frand(-60, 60);
            cy = frand(-30, 40);
            cz = frand(-60, 60);
            tries++;
        } while (overlaps(cx, cy, cz, r) && tries < 800);

        add_bound(cx, cy, cz, r);

        float ux = frand(4, 10), uy = frand(-2, 2), uz = frand(-1, 1);
        float vx = frand(-1, 1), vy = frand(-2, 2), vz = frand(4, 10);

        int mat = rand() % 7;

        printf(
            "      { \"corner\": [%.3f,%.3f,%.3f], \"u\": [%.3f,%.3f,%.3f], "
            "\"v\": [%.3f,%.3f,%.3f], \"material\": %d }%s\n",
            cx, cy, cz, ux, uy, uz, vx, vy, vz, mat,
            (i < count - 1 ? "," : ""));
    }

    printf("    ],\n");
}

/********************************************************
 * Boxes
 ********************************************************/
void gen_boxes(int count) {
    printf("    \"boxes\": [\n");

    for (int i = 0; i < count; i++) {
        float ax, ay, az, bx, by, bz;
        float cx, cy, cz, r;
        int tries = 0;

        do {
            ax = frand(-40, 30);
            ay = frand(-30, 30);
            az = frand(-40, 30);

            bx = ax + frand(5, 15);
            by = ay + frand(5, 20);
            bz = az + frand(5, 15);

            cx = (ax + bx) * 0.5f;
            cy = (ay + by) * 0.5f;
            cz = (az + bz) * 0.5f;

            r = fmaxf(fmaxf(bx - ax, by - ay), bz - az);
            tries++;
        } while (overlaps(cx, cy, cz, r) && tries < 800);

        add_bound(cx, cy, cz, r);

        int mat = rand() % 7;

        printf(
            "      { \"a\": [%.3f,%.3f,%.3f], \"b\": [%.3f,%.3f,%.3f], "
            "\"material\": %d }%s\n",
            ax, ay, az, bx, by, bz, mat, (i < count - 1 ? "," : ""));
    }

    printf("    ]\n");
}

/********************************************************
 * Main — Outputs complete JSON Scene
 ********************************************************/
int main() {
    srand(12345);  // deterministic

    printf("{\n");

    /************ CONFIG ************/
    printf(
        "  \"config\": {\n"
        "    \"width\": 3840,\n"
        "    \"height\": 2160,\n"
        "    \"samples_per_pixel\": 500,\n"
        "    \"max_depth\": 50\n"
        "  },\n");

    /************ CAMERA ************/
    printf(
        "  \"camera\": {\n"
        "    \"position\": [0, 0, 8],\n"
        "    \"look_at\": [0, 0, 0],\n"
        "    \"up\": [0, 1, 0],\n"
        "    \"fov\": 90,\n"
        "    \"aspect_ratio\": \"16/9\",\n"
        "    \"defocus_angle\": 0.1,\n"
        "    \"focus_dist\": 4.0\n"
        "  },\n");

    /************ MATERIALS ************/
    printf(
        "  \"materials\": [\n"
        "    { \"type\": \"lambertian\", \"albedo\": [0.8, 0.8, 0.0] },\n"
        "    { \"type\": \"lambertian\", \"albedo\": [0.1, 0.3, 0.5] },\n"
        "    { \"type\": \"dielectric\", \"refraction_index\": 1.5 },\n"
        "    { \"type\": \"metal\", \"albedo\": [0.8, 0.6, 0.8], \"fuzz\": "
        "0.05 },\n"
        "    { \"type\": \"metal\", \"albedo\": [0.9, 0.4, 0.6], \"fuzz\": "
        "0.05 },\n"
        "    { \"type\": \"lambertian\", \"albedo\": [0.5, 0.3, 0.1] },\n"
        "    { \"type\": \"metal\", \"albedo\": [1.0, 1.0, 1.0], \"fuzz\": 0.1 "
        "}\n"
        "  ],\n");

    /************ OBJECTS ************/
    printf("  \"objects\": {\n");

    gen_spheres(300);
    gen_triangles(80);
    gen_quads(40);
    gen_boxes(10);

    printf("  }\n");
    printf("}\n");

    return 0;
}
