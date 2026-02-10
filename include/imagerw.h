#pragma once

#include <stddef.h>
#include <stdint.h>

void export_image(const char *output_file_name, const uint32_t *image,
                  const size_t width, const size_t height);

void export_ppm(const char *output_file_name, const uint32_t *image,
                const size_t width, const size_t height);

void export_png(const char *output_file_name, const uint32_t *image,
                const size_t width, const size_t height);
