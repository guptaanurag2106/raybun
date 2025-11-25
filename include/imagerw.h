#pragma once

#include <stdint.h>

void export_ppm(const char *output_file_name, const uint32_t *image,
                const int width, const int height);

void export_png(const char *output_file_name, const uint32_t *image,
                const int width, const int height);
