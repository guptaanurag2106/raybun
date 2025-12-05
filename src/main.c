#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "imagerw.h"
#include "renderer.h"
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
    char *output_name = "data/simple_scene.ppm";

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
                scene_json_file = "data/benchmark.json";
            } else {
                scene_json_file = shift(&argc, &argv);
            }
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

    JSON json = load_scene(scene_json_file);
    print_summary(json);
    Scene scene = json.scene;
    State state = json.state;

    srand(time(NULL));

    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    calculate_camera_fields(&scene.camera);
    render_scene(&scene, &state);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);

    if (mode == 3) {
        double seconds = diff.tv_sec + diff.tv_usec * 1e-6;
        float tmin = 5;
        float tmax = 20;
        float perf_score = -1;
        if (seconds >= tmax)
            perf_score = 0;
        else if (seconds <= tmin)
            perf_score = 10;
        else
            perf_score = 10.0 * (1.0 - (seconds - tmin) / (tmax - tmin));
        Log(Log_Info,
            temp_sprintf(
                "Benchmark results: Completed in %fs, Performance Score: %f/10",
                seconds, perf_score));
    }

    if (mode != 3) {
        export_image(output_name, state.image, state.width, state.height);
    }

    return 0;
}
