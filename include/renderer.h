#pragma once

#include "scene.h"

typedef V3f Colour;
#define TILE_WIDTH 64
#define TILE_HEIGHT 64
typedef struct {
    int x, y;
    int tw, th;
} Tile;

typedef struct {
    const Scene scene;
    const int tile_count;
    const Tile *tiles;
    _Atomic int tile_finished;

    uint32_t *image;
    _Atomic int ray_count;

    int width;
    int samples_per_pixel;
    int max_depth;

    V3f pixel00_loc;
    V3f pixel_delta_u, pixel_delta_v;
    V3f defocus_disk_u, defocus_disk_v;
    float colour_contribution;
} Work;  // sent to individual thread

void calculate_camera_fields(Camera *cam);
void render_scene(Scene *scene, State *state);

// thread 5 11.9, 1 9.2s

