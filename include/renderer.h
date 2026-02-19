#pragma once

#include <stdint.h>

#include "scene.h"

// Forward-declare MasterState
struct MasterState;

typedef V3f Colour;

#define TILE_WIDTH 64
#define TILE_HEIGHT 64

typedef struct {
    _Atomic long ray_count;
    _Atomic int tile_finished;

    size_t width;
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

void render_single_tile(const Scene *scene, const Tile *tile, const Camera *cam,
                        int samples_per_pixel, int max_depth,
                        const V3f *pixel00_loc, const V3f *pixel_delta_u,
                        const V3f *pixel_delta_v, const V3f *defocus_disk_u,
                        const V3f *defocus_disk_v, float colour_contribution,
                        int image_width, uint32_t *output_buffer);

void compute_render_camera_fields(const Camera *cam, size_t image_width,
                                  size_t image_height, V3f *pixel00_loc,
                                  V3f *pixel_delta_u, V3f *pixel_delta_v,
                                  V3f *defocus_disk_u, V3f *defocus_disk_v);

void render_scene_distributed(struct MasterState *master_state,
                              long thread_count);
