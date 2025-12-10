#include "imagerw.h"

#include <stdio.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "utils.h"

void export_ppm(const char *output_file_name, const uint32_t *image,
                const int width, const int height) {
    FILE *f = fopen(output_file_name, "wb");
    if (f == NULL) {
        Log(Log_Error, temp_sprintf("export_ppm: %s", strerror(errno)));
        exit(1);
    }

    fprintf(f, "P6\n");
    fprintf(f, "%d %d\n", width, height);
    fprintf(f, "255\n");

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            uint32_t pixel = image[i * width + j];

            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;

            fwrite(&r, sizeof(uint8_t), 1, f);
            fwrite(&g, sizeof(uint8_t), 1, f);
            fwrite(&b, sizeof(uint8_t), 1, f);
        }
    }
    fclose(f);
}

void export_png(const char *output_file_name, const uint32_t *image,
                const int width, const int height) {
    uint32_t *converted_image =
        (uint32_t *)malloc(width * height * sizeof(uint32_t));
    if (converted_image == NULL) {
        Log(Log_Error, temp_sprintf("export_png: Memory allocation failed"));
        exit(1);
    }

    for (int i = 0; i < width * height; i++) {
        uint32_t pixel = image[i];
        uint8_t a = (pixel >> 24) & 0xFF;
        uint8_t r = (pixel >> 16) & 0xFF;
        uint8_t g = (pixel >> 8) & 0xFF;
        uint8_t b = (pixel >> 0) & 0xFF;

        converted_image[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    stbi_write_png(output_file_name, width, height, 4, (void *)converted_image,
                   width * 4);

    free(converted_image);
}

void export_image(const char *output_file_name, const uint32_t *image,
                  const int width, const int height) {
    if (strstr(output_file_name, ".png") != NULL) {
        export_png(output_file_name, image, width, height);
    } else if (strstr(output_file_name, ".ppm") != NULL) {
        export_ppm(output_file_name, image, width, height);
    } else {
        Log(Log_Warn,
            "Output format not supported, outputting ppm instead with the same "
            "name");
        export_ppm(output_file_name, image, width, height);
    }

    Log(Log_Info, temp_sprintf("export_image: Successfully written %s",
                               output_file_name));
}
