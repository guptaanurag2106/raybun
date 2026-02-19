#pragma once

#include "renderer.h"
#include "scene.h"
#include "state.h"

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

Vector(MachineInfo, Machines);

typedef struct {
    Scene *scene;
    State *state;
    Machines workers;
    Work *work;
    struct MasterState *master_state;
} MasterAPIContext;

typedef struct MasterState {
    Scene *scene;
    State *state;

    // Tile tracking
    TileAssignment *tiles;
    int tile_count;

    // Registered workers (empty in standalone)
    Machines workers;

    // Render params
    int samples_per_pixel;
    int max_depth;
    float colour_contribution;

    // Camera data for workers
    V3f pixel00_loc;
    V3f pixel_delta_u;
    V3f pixel_delta_v;
    V3f defocus_disk_u;
    V3f defocus_disk_v;

    // Shared image buffer (master writes directly)
    uint32_t *image;
    size_t image_width;

    // Atomic lock for tile assignment updates
    _Atomic int tile_assign_lock;
} MasterState;

typedef struct WorkerState {
    // Worker connection info
    char *master_url;
    char *worker_name;

    // Local scene copy
    Scene *scene;
    State *state;

    // Current assignment
    int assigned_tile_id;
    Tile assigned_tile;

    // Render params received from master
    int samples_per_pixel;
    int max_depth;
    float colour_contribution;
    V3f pixel00_loc;
    V3f pixel_delta_u;
    V3f pixel_delta_v;
    V3f defocus_disk_u;
    V3f defocus_disk_v;

    // Local tile buffer (reuse per assignment)
    uint32_t *tile_buffer;

    // Networking client (curl)
    void *curl_handle;

    // Failure tracking
    int consecutive_failures;
    int is_online;
} WorkerState;

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
bool worker_connect(const char *master_ip, int port, MachineInfo stats);
