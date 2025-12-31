#pragma once

#include "renderer.h"
#include "scene.h"
#include "state.h"
#include "vec.h"

#define SMALL_THRESHOLD (1024 * 1024)     // 1 MB
#define MAX_PAYLOAD (64UL * 1024 * 1024)  // 64 MB absolute cap

typedef struct {
    size_t received;
    size_t content_length;
    int use_tempfile;

    char *buffer;
    size_t buffer_size;

    FILE *tmpfile;

    int processed;
} ConnectionInfo;

typedef struct {
    Scene *scene;
    State *state;
    VECTOR(MachineInfo) workers;
    Work *work;
} MasterAPIContext;

// POST /api/register WorkerHello <-> MasterRegister
typedef MachineInfo WorkerHello;
typedef struct {
    int success;  // 1 for OK 0 for Not
} MasterRegister;

// GET /api/scene GetScene <-> NULL
typedef struct {
    unsigned int scene_crc;
    char *scene_json;
} GetScene;

// GET /api/work?worker_id=<id>&scene_crc=<crc> WorkerWork
typedef struct {
    int tile_id;
    Tile tile;
} WorkerWork;

// POST /api/result
typedef struct {
    char *name;  // worker name
    int tile_id;
    uint32_t *pixels;
} WorkerResult;

// Master
static struct MHD_Daemon *master_daemon;
bool master_start_server(int port, MasterAPIContext *context);

// Worker
static struct MHD_Daemon *worker_daemon;
bool worker_connect(const char *master_ip, int port);
