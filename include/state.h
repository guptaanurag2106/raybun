#pragma once

#include <stdint.h>

typedef struct {
    int width;
    int height;
    int samples_per_pixel;
    int max_depth;

    uint32_t *image;
} State;
