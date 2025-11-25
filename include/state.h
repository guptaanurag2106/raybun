#pragma once

#include <stdint.h>

typedef struct {
    int width;
    int height;

    uint32_t *image;
} State;
