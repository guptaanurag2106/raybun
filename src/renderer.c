#include "renderer.h"

#include <stdint.h>

#include "scene.h"
#include "vec.h"

uint32_t pack_colour(float r, float g, float b, float a) {
    return (((uint8_t)(a * 255)) << 24) | (((uint8_t)(r * 255)) << 16) |
           (((uint8_t)(g * 255)) << 8) | ((uint8_t)(b * 255));
}

void calculate_camera_fields(Camera *cam) {
    // TODO: see what is needed
    cam->forward = v3f_normalize(v3f_sub(cam->look_at, cam->position));
    cam->right = v3f_normalize(v3f_cross(cam->forward, cam->up));
    cam->up = v3f_cross(cam->right, cam->forward);

    cam->viewport_h = 2 * tanf(cam->fov / 2.0f);
    cam->viewport_w = cam->aspect_ratio * cam->viewport_h;
    cam->focal_length = 1 / tanf(cam->fov / 2);
}

Hit hit_sphere(Ray ray, Scene *scene) {
    for (int i = 0; i < scene->sphere_count; i++) {
        Sphere sphere = scene->spheres[i];
        v3f oc = v3f_sub(sphere.center, ray.origin);
        float a = v3f_slength(ray.direction);
        float h = v3f_dot(ray.direction, oc);
        float c = v3f_slength(oc) - sphere.radius * sphere.radius;

        float discriminant = (h * h - a * c);
        if (discriminant >= 0) {
            float t = (h - sqrtf(discriminant)) / a;
            if (t < 0) {
                t = (h + sqrtf(discriminant)) / a;
                if (t > 0) {  // TODO: >0 or some near_point?
                    return (Hit){i, t};
                }
            } else {
                return (Hit){
                    i, t};  // FIX: hit first sphere at first point, incorrect,
                            // check minimum t across all spheres
            }
        }
    }
    return (Hit){-1, -1};
}

uint32_t ray_colour(Ray ray, Scene *scene) {
    Hit hit_s = hit_sphere(ray, scene);
    uint32_t colour;
    if (hit_s.sphere_index != -1) {
        v3f N = v3f_normalize(v3f_sub(ray_at(ray, hit_s.t), (v3f){0, 0, -1}));
        colour =
            pack_colour(0.5 * (1 + N.x), 0.5 * (1 + N.y), 0.5 * (1 + N.z), 1);
    } else {
        float a = 0.5 * (v3f_normalize(ray.direction).y + 1);
        colour = pack_colour(1 - 0.5 * a, 1 - 0.3 * a, 1, 1);
    }
    return colour;
}

void render_scene(Scene *scene, State *state) {
    const int width = state->width;
    const int height = state->height;

    Camera cam = scene->camera;

    v3f vu = {cam.viewport_w, 0, 0};
    v3f vv = {0, -cam.viewport_h, 0};

    v3f pixel_delta_u = v3f_divf(vu, width);
    v3f pixel_delta_v = v3f_divf(vv, height);

    v3f viewport_top_left =
        v3f_sub(v3f_add(cam.position, v3f_mulf(cam.forward, cam.focal_length)),
                v3f_add(v3f_divf(vu, 2), v3f_divf(vv, 2)));

    v3f pixel00_loc =
        v3f_add(viewport_top_left,
                v3f_mulf(v3f_add(pixel_delta_u, pixel_delta_v), 0.5));

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            v3f pixel_center =
                v3f_add(pixel00_loc, v3f_add(v3f_mulf(pixel_delta_u, i),
                                             v3f_mulf(pixel_delta_v, j)));
            Ray ray = {cam.position, v3f_sub(pixel_center, cam.position)};
            state->image[j * width + i] = ray_colour(ray, scene);
        }
    }
}
