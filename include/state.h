#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t width;
    size_t height;
    int samples_per_pixel;
    int max_depth;

    uint32_t *image;
} State;

typedef struct {
    float perf;
    long thread_count;
    int simd;  // future use

    char *name;
} MachineInfo;
