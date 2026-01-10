#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "api.h"
#include "imagerw.h"
#include "libmicrohttpd-1.0.1/src/include/microhttpd.h"
#include "renderer.h"
#include "scene.h"
#include "state.h"
#define UTILS_IMPLEMENTATION
#include "utils.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

const enum Log_Level level = Log_Trace;

void usage(const char *prog_name) {
    printf("%s — Distributed and standalone renderer\n\n", prog_name);
    printf("Usage:\n");
    printf("  %s <COMMAND> [ARGS]\n\n", prog_name);
    printf("Commands:\n");
    printf("  master <PORT> <SCENE> [OUTPUT]\n");
    printf("      Coordinate workers and manage the render queue\n\n");
    printf("  worker <MASTER_URL> [DEVICE_ID]\n");
    printf("      Connect to master and render assigned tiles\n\n");
    printf("  standalone <SCENE> [OUTPUT]\n");
    printf("      Render locally (single process)\n\n");
    printf("  benchmark <SCENE>\n");
    printf("      Run performance benchmark on this machine\n\n");
    printf("Arguments:\n");
    printf("  PORT           Port to listen on (master mode)\n");
    printf("  SCENE          Scene description file (JSON)\n");
    printf("  OUTPUT         Output image file name\n");
    printf("  MASTER_URL     Master address (e.g. http://127.0.0.1:8080)\n");
    printf("  DEVICE_ID      ID for the worker device\n\n");
    printf("Options:\n");
    printf("  -h, --help     Print this help message and exit\n");
}

void print_args_error(const char *prog_name, const char *error) {
    fprintf(stderr, "error: %s\n", error);
    fprintf(stderr, "For more information, try '%s -h'.\n", prog_name);
    exit(1);
}

MachineInfo get_device_stats(const char *perf_json_file, char *name) {
    Log_set_level(Log_Warn);

    Scene *scene = malloc(sizeof(Scene));
    memset(scene, 0, sizeof(Scene));
    State *state = malloc(sizeof(State));

    char *file_content = read_compress_scene(perf_json_file);
    load_scene(file_content, scene, state);
    free(file_content);

    Work *work = malloc(sizeof(Work));

    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    calculate_camera_fields(&scene->camera);
    init_work(scene, state, work);
    render_scene(work, 1);  // get single threaded performance

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);

    float perf_score = -1;
    float ms = diff.tv_sec * 1000 + diff.tv_usec * 1e-3;
    float tmin = 1000;
    float tmax = 10000;
    if (ms >= tmax)
        perf_score = 0;
    else if (ms <= tmin)
        perf_score = 10;
    else
        perf_score = 10.0 * (1.0 - (ms - tmin) / (tmax - tmin));

    free_scene(scene);
    Log_set_level(level);

    int thread_count = sysconf(_SC_NPROCESSORS_ONLN);
    int simd = -1;
    if (name == NULL || strlen(name) == 0) {
        name = generate_uuid();
    }
    MachineInfo stats = {.perf = perf_score,
                         .thread_count = thread_count,
                         .simd = simd,
                         .name = name};

    Log(Log_Info,
        "Benchmark results for %s: Rendered %s in %.0fms, "
        "Performance Score: "
        "%.2f/10, Thread Count: %d, SIMD type %d.",
        name, perf_json_file, ms, perf_score, thread_count, simd);
    return stats;
}

int main(int argc, char **argv) {
    char *prog_name = shift(&argc, &argv);

    if (argc == 0) {
        usage(prog_name);
        return 0;
    }

    int mode = 0;
    char *scene_json_file = "data/simple_scene.json";
    char *perf_json_file = "data/benchmark.json";
    char *output_name = "data/simple_scene.ppm";

    int port;
    char *master_url = "";
    char *device_name = NULL;

    while (argc > 0) {
        char *flag = shift(&argc, &argv);

        if (strncmp(flag, "master", 6) == 0) {
            if (argc <= 0) {
                print_args_error(
                    prog_name, "missing required arguments <PORT> and <SCENE>");
            }
            const char *port1 = shift(&argc, &argv);
            port = atoi(port1);
            if (port == 0) {
                print_args_error(
                    prog_name,
                    temp_sprintf("invalid value '%s' for <PORT>", port1));
            }
            if (argc <= 0) {
                print_args_error(prog_name,
                                 "missing required argument <SCENE>");
            }
            scene_json_file = shift(&argc, &argv);
            if (argc > 0) {
                output_name = shift(&argc, &argv);
            }
            mode = 0;
        } else if (strncmp(flag, "worker", 6) == 0) {
            if (argc <= 0) {
                print_args_error(prog_name,
                                 "missing required argument <MASTER_URL>");
            }
            perf_json_file = "data/benchmark.json";  // used for benchmark
            master_url = shift(&argc, &argv);
            if (argc > 0) device_name = shift(&argc, &argv);
            mode = 1;
        } else if (strncmp(flag, "standalone", 10) == 0) {
            if (argc <= 0) {
                print_args_error(prog_name,
                                 "missing required argument <SCENE>");
            }
            scene_json_file = shift(&argc, &argv);
            if (argc > 0) {
                output_name = shift(&argc, &argv);
            }
            mode = 2;
        } else if (strncmp(flag, "benchmark", 9) == 0) {
            if (argc > 0) {
                perf_json_file = shift(&argc, &argv);
            }
            mode = 3;
        } else if (strncmp(flag, "-h", 2) == 0 ||
                   strncmp(flag, "--help", 6) == 0) {
            usage(prog_name);
            return 0;
        } else {
            print_args_error(prog_name,
                             temp_sprintf("unexpected option '%s'", flag));
        }
    }
    UNUSED(master_url);

    MachineInfo stats = get_device_stats(perf_json_file, device_name);

    if (mode == 3) {
        exit(0);
    }

    Scene *scene = NULL;
    State *state = NULL;
    char *scene_json = NULL;
    if (mode == 0 || mode == 2) {
        scene = malloc(sizeof(Scene));
        memset(scene, 0, sizeof(Scene));
        state = malloc(sizeof(State));

        scene_json = read_compress_scene(scene_json_file);
        unsigned int scene_crc =
            stbiw__crc32((unsigned char *)scene_json, strlen(scene_json));

        load_scene(scene_json, scene, state);
        scene->scene_crc = scene_crc;
        scene->scene_json = scene_json;
        print_summary(scene, state);
        calculate_camera_fields(&scene->camera);

        MasterAPIContext *context = malloc(sizeof(MasterAPIContext));
        if (!context) return false;
        context->scene = scene;
        context->state = state;
        vec_init(&context->workers);

        context->work = malloc(sizeof(Work));

        init_work(context->scene, context->state, context->work);

        // TODO: better error handling, check async
        //  Start server before rendering so we can check status/connect
        if (mode == 0) {
            bool success = master_start_server(port, context);
            if (success) {
                Log(Log_Info, "master_start_server: Started master server");
            } else {
                Log(Log_Error,
                    "master_start_server: Cannot start master server");
            }
        }

        // master will run on N-1 threads
        int thread_count = stats.thread_count - 1;
        render_scene(context->work, thread_count);
        vec_free(&context->workers);
    }

    if (mode == 1) {
        bool success = worker_connect("", 1);
        if (success) {
            Log(Log_Info, "worker_connect: Connected to master");
        } else {
            Log(Log_Error, "worker_connect: Cannot connect to master");
        }
    }

    // export image for master, and standalone modes
    if (mode == 0 || mode == 2) {
        export_image(output_name, state->image, state->width, state->height);
    }

    Log(Log_Info, "Press Enter to exit...");
    // getchar();
    if (master_daemon) {
        MHD_stop_daemon(master_daemon);
    }
    if (worker_daemon) {
        MHD_stop_daemon(worker_daemon);
    }

    free(scene_json);
    free_scene(scene);

    return 0;
}
