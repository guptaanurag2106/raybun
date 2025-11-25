#pragma once

#include "vec.h"

typedef struct {
    v3f position;
    v3f look_at;
    int fov;
} Camera;
