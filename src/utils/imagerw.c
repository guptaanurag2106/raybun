#include "imagerw.h"

#include <stdio.h>
#include <string.h>

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

    Log(Log_Info,
        temp_sprintf("export_ppm: successfully written %s", output_file_name));
}
