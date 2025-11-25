#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imagerw.h"
#include "scene.h"
#include "state.h"
#define UTILS_IMPLEMENTATION
#include "utils.h"

void usage(const char *prog_name) {
    printf("Usage: %s <mode> [options]\n", prog_name);
    printf("\nModes:\n");
    printf("  master <port> <scene.json> [optional_output_image_name]\n");
    printf("    Coordinate workers, manage job queue\n");
    printf("  worker <master_ip:port>\n");
    printf("    Poll for work, render tiles\n");
    printf("  standalone <scene.json> [optional_output_image_name]\n");
    printf("    Render locally (for testing)\n");
    printf("  benchmark <scene.json>\n");
    printf("    Test performance on this machine\n\n");
    printf("-h/--help Print this help message and exit\n");
}

void print_args_error(const char *prog_name, const char *error) {
    fprintf(stderr, "%s\n", error);
    fprintf(stderr, "More info with %s -h\n", prog_name);
    exit(1);
}

int main(int argc, char **argv) {
    char *prog_name = shift(&argc, &argv);

    if (argc == 0) {
        usage(prog_name);
        return 0;
    }

    int mode = 0;
    char *scene_json_file = "";
    char *output_name = "output.ppm";

    int port;
    char *master_ip = "";

    while (argc > 0) {
        char *flag = shift(&argc, &argv);

        if (strncmp(flag, "master", 6) == 0) {
            if (argc <= 0) {
                print_args_error(
                    prog_name,
                    "subcommand 'master': expected a port and scene.json file");
            }
            const char *port1 = shift(&argc, &argv);
            port = atoi(port1);
            if (port == 0) {
                print_args_error(
                    prog_name,
                    temp_sprintf(
                        "subcommand 'master': could not parse port '%s'",
                        port1));
            }
            if (argc <= 0) {
                print_args_error(
                    prog_name,
                    "subcommand 'master': expected a scene.json file");
            }
            scene_json_file = shift(&argc, &argv);
            if (argc > 0) {
                output_name = shift(&argc, &argv);
            }
            mode = 0;
        } else if (strncmp(flag, "worker", 6) == 0) {
            if (argc <= 0) {
                print_args_error(
                    prog_name,
                    "subcommand 'worker': expected a master_ip:port");
            }
            master_ip = shift(&argc, &argv);
            mode = 1;
        } else if (strncmp(flag, "standalone", 10) == 0) {
            if (argc <= 0) {
                print_args_error(
                    prog_name,
                    "subcommand 'standalone': expected scene.json file");
            }
            scene_json_file = shift(&argc, &argv);
            if (argc > 0) {
                output_name = shift(&argc, &argv);
            }
            mode = 2;
        } else if (strncmp(flag, "benchmark", 9) == 0) {
            if (argc <= 0) {
                print_args_error(
                    prog_name,
                    "subcommand 'benchmark': expected scene.json file");
            }
            scene_json_file = shift(&argc, &argv);
            mode = 3;
        } else if (strncmp(flag, "-h", 2) == 0 ||
                   strncmp(flag, "--help", 6) == 0) {
            usage(prog_name);
            return 0;
        } else {
            print_args_error(prog_name,
                             temp_sprintf("Unknown option '%s'", flag));
        }
    }

    printf("%s %s %d %s %d\n", scene_json_file, output_name, port, master_ip,
           mode);

    JSON json = load_scene(scene_json_file);
    Scene scene = json.scene;
    State state = json.state;

    for (int i = 0; i < state.width * state.height; i++) {
        if (i < 10000)
            state.image[i] = 0xFF0000FF;
        else
            state.image[i] = 0xFF00FF00;
    }

    export_ppm(output_name, state.image, state.width, state.height);

    return 0;
}
