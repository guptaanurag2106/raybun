#pragma once

#include <stdint.h>

#include "scene.h"

typedef V3f Colour;
typedef struct {
    int x, y;
    int tw, th;
} Tile;

typedef struct {
    _Atomic long ray_count;
    _Atomic int tile_finished;
    char _pad[64 - sizeof(_Atomic long) - sizeof(_Atomic int)];

    const Scene *scene;
    const int tile_count;
    const Tile *tiles;

    uint32_t *image;

    int width;
    int samples_per_pixel;
    int max_depth;

    const V3f pixel00_loc;
    const V3f pixel_delta_u, pixel_delta_v;
    const V3f defocus_disk_u, defocus_disk_v;
    const float colour_contribution;
} Work;  // sent to individual thread

void calculate_camera_fields(Camera *cam);
void render_scene(Scene *scene, State *state);
