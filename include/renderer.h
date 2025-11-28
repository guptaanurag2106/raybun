#pragma once

#include "scene.h"

typedef v3f Colour;

void calculate_camera_fields(Camera *cam);
void render_scene(Scene *scene, State *state);
