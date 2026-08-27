#include <stdio.h>

#define UTILS_IMPLEMENTATION
#include "utils.h"
#define ARENA_IMPLEMENTATION
#define JSON_IMPLEMENTATION
#include "json.h"

char *to_upper(const char *str) {
    char *out = strdup(str);
    for (size_t i = 0; i < strlen(str); i++) {
        if (out[i] >= 'a' && out[i] <= 'z') {
            out[i] -= 'a' - 'A';
        }
    }
    return out;
}

int main(int argc, char **argv) {
    argc--;
    argv++;
    if (argc == 0) return 0;
    char *file_name = *argv;

    char *content = read_entire_file(file_name);
    if (content == NULL) return 0;
    minify_str(content);
    char *escaped = escape_str(content);
    const char *name = "benchmark_json";
    char *upper = to_upper(name);

    printf("#ifndef %s_H\n", upper);
    printf("#define %s_H\n", upper);
    printf("const char *%s_content = \"%s\";\n", name, escaped);
    printf("#endif //%s_H", upper);

    free(content);
    free(escaped);
    free(upper);

    return 0;
}
