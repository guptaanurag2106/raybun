#include "renderer.h"

#include <malloc.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"
#include "rinternal.h"
#include "scene.h"
#include "utils.h"
#include "vec.h"

static inline float linear_to_gamma(float linear_component) {
    if (linear_component > 0) return sqrtf(linear_component);
    return 0;
}

// argb
uint32_t pack_colour(Colour colour) {
    colour = v3f_clamp(colour, 0, 1);
    return (((uint8_t)(255)) << 24) |
           (((uint8_t)(linear_to_gamma(colour.x) * 255)) << 16) |
           (((uint8_t)(linear_to_gamma(colour.y) * 255)) << 8) |
           ((uint8_t)(linear_to_gamma(colour.z) * 255));
}

void calculate_camera_fields(Camera *cam) {
    cam->forward = v3f_normalize(v3f_sub(cam->look_at, cam->position));
    cam->right = v3f_normalize(v3f_cross(cam->forward, cam->up));
    cam->up = v3f_cross(cam->right, cam->forward);

    cam->viewport_h = 2 * tanf(cam->fov / 2.0f) * cam->focus_dist;
    cam->viewport_w = cam->aspect_ratio * cam->viewport_h;
    cam->focal_length = 1 / tanf(cam->fov / 2);
}

const Colour BACKGROUND = {0.1, 0.1, 0.1};
Colour ray_colour(Ray *ray, const Scene *scene, int depth, int *ray_count) {
    (*ray_count)++;
    if (depth <= 0) return (Colour){0, 0, 0};
    HitRecord record = {0};
    if (scene_hit(ray, scene, 0.001, INFINITY,
                  &record)) {  // 0.001: floating pound rounding error, if the
                               // origin is too close to surface then
                               // intersection point may be inside the surface
                               // then the ray will just bounce inside
        Ray scattered;
        Colour attenuation;
        Colour colour_emission = ORIGIN;

        Material *mat = &scene->materials[record.mat_index];
        if (mat->type == MAT_EMISSIVE) {
            colour_emission = mat->properties.emissive.emission;
        }

        if (!scatter(mat, &record, ray, &attenuation, &scattered)) {
            return colour_emission;
        }

        Colour scatter = v3f_comp_mul(
            attenuation, ray_colour(&scattered, scene, depth - 1, ray_count));
        return v3f_add(colour_emission, scatter);
    }

    return BACKGROUND;
}

void *render_tile(void *arg) {
    Work *work = arg;
    const Scene *scene = work->scene;
    Camera cam = scene->camera;

    int ray_count = 0;
    int curr_tile;
    rng_seed_tls((uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)pthread_self());

    while (true) {
        curr_tile = atomic_fetch_add(&work->tile_finished, 1);
        if (curr_tile >= work->tile_count) break;

        Tile tile = work->tiles[curr_tile];
        V3f row_start = v3f_add(work->pixel00_loc,
                                v3f_add(v3f_mulf(work->pixel_delta_u, tile.x),
                                        v3f_mulf(work->pixel_delta_v, tile.y)));

        for (int j = tile.y; j < tile.y + tile.th; j++) {
            for (int i = tile.x; i < tile.x + tile.tw; i++) {
                Colour colour = (V3f){0, 0, 0};

                V3f pixel_base = v3f_add(
                    row_start,
                    v3f_add(v3f_mulf(work->pixel_delta_u, (i - tile.x)),
                            v3f_mulf(work->pixel_delta_v, (j - tile.y))));

                for (int s = 0; s < work->samples_per_pixel; s++) {
                    V3f pixel_center = v3f_add(
                        pixel_base, v3f_add(v3f_mulf(work->pixel_delta_u,
                                                     rng_f32_tls() - 0.5f),
                                            v3f_mulf(work->pixel_delta_v,
                                                     rng_f32_tls() - 0.5f)));

                    V3f ray_origin;
                    if (cam.defocus_angle <= 0) {
                        ray_origin = cam.position;
                    } else {
                        V3f p = v3f_random_in_unit_disk();
                        ray_origin = v3f_add(
                            cam.position,
                            v3f_add(v3f_mulf(work->defocus_disk_u, p.x),
                                    v3f_mulf(work->defocus_disk_v, p.y)));
                    }
                    Ray ray = {ray_origin, v3f_sub(pixel_center, cam.position)};
                    ray.inv_dir = v3f_inv(ray.direction);
                    colour = v3f_add(
                        ray_colour(&ray, scene, work->max_depth, &ray_count),
                        colour);
                }
                work->image[j * work->width + i] =
                    pack_colour(v3f_mulf(colour, work->colour_contribution));
            }
        }
    }
    atomic_fetch_add(&work->ray_count, ray_count);

    pthread_exit(NULL);
}

void render_scene(Scene *scene, State *state) {
    const int width = state->width;
    const int height = state->height;

    Camera cam = scene->camera;

    V3f vu = v3f_mulf(cam.right, cam.viewport_w);
    V3f vv = v3f_mulf(cam.up, -cam.viewport_h);

    V3f pixel_delta_u = v3f_divf(vu, width);
    V3f pixel_delta_v = v3f_divf(vv, height);

    V3f viewport_top_left =
        v3f_sub(v3f_add(cam.position, v3f_mulf(cam.forward, cam.focus_dist)),
                v3f_add(v3f_divf(vu, 2), v3f_divf(vv, 2)));

    V3f pixel00_loc =
        v3f_add(viewport_top_left,
                v3f_mulf(v3f_add(pixel_delta_u, pixel_delta_v), 0.5));

    float defocus_radius = cam.focus_dist * tanf(cam.defocus_angle / 2);
    V3f defocus_disk_u = v3f_mulf(cam.right, defocus_radius);
    V3f defocus_disk_v = v3f_mulf(cam.up, defocus_radius);

#define TILE_WIDTH width
#define TILE_HEIGHT 128

    const int tile_count =
        CEILF((float)width / TILE_WIDTH) * CEILF((float)height / TILE_HEIGHT);

    Tile *tiles = malloc(sizeof(*tiles) * tile_count);
    Log(Log_Info, temp_sprintf("Breaking into %d tiles", tile_count));
    if (tiles == NULL) {
        Log(Log_Warn, "render_scene: Could not allocate tiles");
        exit(1);
    }

    int count = 0;
    for (int j = 0; j < height; j += TILE_HEIGHT) {
        for (int i = 0; i < width; i += TILE_WIDTH) {
            tiles[count++] = (Tile){
                .x = i,
                .y = j,
                .tw = (i + TILE_WIDTH > width) ? (width - i) : TILE_WIDTH,
                .th = (j + TILE_HEIGHT > height) ? (height - j) : TILE_HEIGHT};
        }
    }

    const float colour_contribution = 1.0f / state->samples_per_pixel;

    Work work = {
        .scene = scene,
        .tile_count = tile_count,
        .tiles = tiles,
        .tile_finished = 0,

        .image = state->image,
        .ray_count = 0,

        .width = width,
        .samples_per_pixel = state->samples_per_pixel,
        .max_depth = state->max_depth,

        .pixel00_loc = pixel00_loc,
        .pixel_delta_u = pixel_delta_u,
        .pixel_delta_v = pixel_delta_v,
        .defocus_disk_u = defocus_disk_u,
        .defocus_disk_v = defocus_disk_v,
        .colour_contribution = colour_contribution,
    };

    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    // int cores = sysconf(_SC_NPROCESSORS_ONLN);
    // int thread_count = MAX(1, cores - 1);
    int thread_count = 6;
    // TODO: thread pinning, automatically get thread count
    Log(Log_Info, temp_sprintf("Running over %d threads", thread_count));
    pthread_t thread[thread_count];
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&thread[i], NULL, render_tile, &work);
        // cpu_set_t set;
        // CPU_ZERO(&set);
        // CPU_SET(i, &set);
        // pthread_setaffinity_np(thread[i], sizeof(set), &set);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(thread[i], NULL);
    }

    int ray_count = work.ray_count;
    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);

    float ms = diff.tv_sec * 1000 + diff.tv_usec * 1e-3;
    double time_per_ray = ms / ray_count;

    Log(Log_Info, temp_sprintf("Rendered %d rays in %ldms or %fms/ray",
                               ray_count, (long int)ms, time_per_ray));
}
