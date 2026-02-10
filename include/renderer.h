#pragma once

#include <stdint.h>

#include "scene.h"

typedef V3f Colour;

#define TILE_WIDTH 64
#define TILE_HEIGHT 64
typedef struct {
    int x, y;
    int tw, th;
} Tile;

typedef struct {
    _Atomic long ray_count;
    _Atomic int tile_finished;

    int width;
    int samples_per_pixel;
    int max_depth;

    Scene *scene;
    int tile_count;
    Tile *tiles;

    uint32_t *image;

    V3f pixel00_loc;
    V3f pixel_delta_u, pixel_delta_v;
    V3f defocus_disk_u, defocus_disk_v;
    float colour_contribution;
} Work;  // sent to individual thread

void calculate_camera_fields(Camera *cam);
void init_work(Scene *scene, State *state, Work *work);
void render_scene(Work *work, long thread_count);
