#ifndef UTILS_H_
#define UTILS_H_

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UTILS_DEF
#ifdef UTILS_IMPLEMENTATION
#define UTILS_DEF
#else
#define UTILS_DEF extern
#endif
#endif

#if defined(_MSC_VER)
#define UTILS_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define UTILS_TLS _Thread_local
#else
#define UTILS_TLS __thread
#endif

// ----------------------------------------------------------------------------
//  Log Utils
// ----------------------------------------------------------------------------
enum Log_Level {
    Log_Trace = 0,
    Log_Debug,
    Log_Info,
    Log_Warn,
    Log_Error,
    Log_Fatal,
    Log_None
};

static enum Log_Level _base_log_level = Log_Trace;

static inline void Log_set_level(enum Log_Level level) {
    _base_log_level = level;
}

UTILS_DEF int Log_set_out_file(const char *out_file);

UTILS_DEF void Log(enum Log_Level level, const char *format, ...);

// ----------------------------------------------------------------------------
//  General Utils
// ----------------------------------------------------------------------------
#ifdef DEBUG
#define ASSERT(x)                                                           \
    do {                                                                    \
        if (!(x)) {                                                         \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #x, __FILE__, \
                    __LINE__);                                              \
            abort();                                                        \
        }                                                                   \
    } while (0)
#else
#define ASSERT(x) ((void)0)
#endif

#define MAX(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _a : _b;      \
    })

#define MIN(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _b : _a;      \
    })

#if defined(__clang__) || defined(__GNUC__)
#define ARRAY_LENGTH(arr)                                             \
    (sizeof(arr) / sizeof((arr)[0]) +                                 \
     0 * sizeof(struct {                                              \
         int _ : !__builtin_types_compatible_p(__typeof__(arr),       \
                                               __typeof__(&(arr)[0])) \
                 ? 1                                                  \
                 : -1;                                                \
     }))
#else
#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define UNREACHABLE(...)                                                  \
    do {                                                                  \
        fprintf(stderr, "[UNREACHABLE]: %s:%d \n" #__VA_ARGS__, __FILE__, \
                __LINE__);                                                \
        exit(1);                                                          \
    } while (0)

#define TODO(...)                                                             \
    do {                                                                      \
        fprintf(stderr, "[TODO]: %s:%d \n" #__VA_ARGS__, __FILE__, __LINE__); \
        exit(1);                                                              \
    } while (0)

#define UNUSED(x) (void)(x)

static inline char *shift(int *argc, char ***argv) {
    ASSERT(*argc > 0);
    (*argc)--;
    return *((*argv)++);
}

UTILS_DEF char *generate_uuid();

static inline void timersub(const struct timeval *a, const struct timeval *b,
                            struct timeval *result) {
    result->tv_sec = a->tv_sec - b->tv_sec;
    result->tv_usec = a->tv_usec - b->tv_usec;
    if (result->tv_usec < 0) {
        result->tv_sec--;
        result->tv_usec += 1000000;
    }
}

// ----------------------------------------------------------------------------
//  Math Utils
// ----------------------------------------------------------------------------
#define PI 3.14159265359f
#define PI_2 (PI / 2)
#define PI_3_4 (3.0f * PI / 4)
#define PI_2_3 (2.0f * PI / 3)
/* #define EPS 1e-8 */

#define DEG2RAD(_d) ((_d) * (PI / 180.0f))
#define RAD2DEG(_r) ((_r) * (180.0f / PI))

#define CEILF(x)                                  \
    ({                                            \
        float _x = (x);                           \
        int _i = (int)_x;                         \
        (_i == _x) ? _i : (_x > 0 ? _i + 1 : _i); \
    })

#define FLOATF(x)                                 \
    ({                                            \
        float _x = (x);                           \
        int _i = (int)_x;                         \
        (_i == _x) ? _i : (_x > 0 ? _i : _i - 1); \
    })

static inline float lerp_float(float start, float end, float t) {
    return start + t * (end - start);
}

#define CLAMP_TYPE(type)                                        \
    static inline type clamp_##type(type v, type lo, type hi) { \
        const type _v = (v);                                    \
        const type _lo = (lo);                                  \
        const type _hi = (hi);                                  \
        if (_v < _lo) return _lo;                               \
        if (_v > _hi) return _hi;                               \
        return _v;                                              \
    }

CLAMP_TYPE(int)
CLAMP_TYPE(float)

UTILS_DEF bool is_number(const char *s);

// warn: no modulus: wrap when just went beyond boundary
#define WRAP_TYPE(type)                                              \
    static inline type wrap_##type(type value, type min, type max) { \
        const type _v = (value);                                     \
        const type _min = (min);                                     \
        const type _max = (max);                                     \
        if (_max <= _min) return _v;                                 \
        if (_v < _min) return _v + (_max - _min);                    \
        if (_v > _max) return _v - (_max - _min);                    \
        return _v;                                                   \
    }

WRAP_TYPE(int)
WRAP_TYPE(float)
WRAP_TYPE(double)

typedef struct RNG {
    uint32_t state;
} RNG;

static RNG rng_state = (RNG){0x12345678u};

static inline void rng_seed(RNG *rng, uint32_t seed) {
    rng->state = seed ? seed : 0x12345678u;
}
static inline void rng_seed_tls(uint32_t seed) {
    rng_state = (RNG){seed ? seed : 0x12345678u};
}

static inline uint32_t rng_u32(RNG *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

static inline uint32_t rng_u32_tls() { return rng_u32(&rng_state); }

static inline float rng_f32(RNG *rng) {
    return (rng_u32(rng) >> 8) * (1.0f / 16777216.0f);
}

static inline float rng_f32_tls() { return rng_f32(&rng_state); }

static inline float rngf_range(RNG *rng, float min, float max) {
    return min + (max - min) * rng_f32(rng);
}

static inline float rngf_range_tls(float min, float max) {
    return rngf_range(&rng_state, min, max);
}

static inline int rngi_range(RNG *rng, int min, int max) {
    return (int)rngf_range(rng, min, max + 1);
}

static inline int rngi_range_tls(int min, int max) {
    return rngi_range(&rng_state, min, max);
}

UTILS_DEF int calculate_infix(const char *expr);  // 34+5*10+3 -> 88 just +-*/%

UTILS_DEF float triangle_area_float(float x1, float y1, float x2, float y2,
                                    float x3, float y3);

UTILS_DEF bool triangle_is_inside(float x1, float y1, float x2, float y2,
                                  float x3, float y3, float x, float y);

#define Vector(T, Name)  \
    typedef struct {     \
        T *items;        \
        size_t size;     \
        size_t capacity; \
    } Name

#define vec_init(v)        \
    do {                   \
        (v)->items = NULL; \
        (v)->size = 0;     \
        (v)->capacity = 0; \
    } while (0)

#define vec__grow(items, capacity, elem_size)                        \
    do {                                                             \
        size_t new_cap = (*(capacity) == 0 ? 1 : (*(capacity) * 2)); \
        void *new_items = realloc(*(items), new_cap * (elem_size));  \
        *(items) = new_items;                                        \
        *(capacity) = new_cap;                                       \
    } while (0)

#define vec_reserve(v, n)                                            \
    do {                                                             \
        (v)->items = realloc((v)->items, (n) * sizeof(*(v)->items)); \
        (v)->capacity = (n);                                         \
    } while (0)

#define vec_push(v, value)                                  \
    do {                                                    \
        if ((v)->size >= (v)->capacity)                     \
            vec__grow((void **)&(v)->items, &(v)->capacity, \
                      sizeof((v)->items[0]));               \
        (v)->items[(v)->size++] = (value);                  \
    } while (0)

// FIX: use ASSERT?
#define vec_get(v, n) (assert((n) < (v)->size), (v)->items[(n)])

#define vec_pop(v) (--(v)->size)

#define vec_back(v) vec_get(v, (v)->size - 1)

#define vec_remove_swap(v, i)                      \
    do {                                           \
        (v)->items[i] = (v)->items[(v)->size - 1]; \
        --(v)->size;                               \
    } while (0)

#define vec_search_first(v, item, cmp_fn)                \
    ({                                                   \
        size_t i = 0;                                    \
        for (; (v)->size; i++) {                         \
            if (cmp_fn(item, (v)->items[i]) == 0) break; \
        }                                                \
        i;                                               \
    })

#define vec_clear(v) ((v)->size = 0)

#define vec_free(v)        \
    do {                   \
        free((v)->items);  \
        (v)->items = NULL; \
        (v)->size = 0;     \
        (v)->capacity = 0; \
    } while (0)

#define vec_release(v)        \
    ({                        \
        void *p = (v)->items; \
        (v)->items = NULL;    \
        (v)->size = 0;        \
        (v)->capacity = 0;    \
        p;                    \
    })

// ----------------------------------------------------------------------------
//  String Utils
// ----------------------------------------------------------------------------
UTILS_DEF char *strdup(const char *src);

#define UTILS_MAX_TEMP_SIZE 1024 * 100
UTILS_DEF char *combine_charp(const char *str1, const char *str2);
// Will use the utils_static_temp_buffer and reset it everytime its filled
#define COMBINE(separator, ...) \
    combine_strings_with_sep_(separator, __VA_ARGS__, NULL)
// Will use the utils_static_temp_buffer and reset it everytime its filled,
// last va_arg should be NULL
UTILS_DEF char *combine_strings_with_sep_(const char *separator, ...);
// Will use the utils_static_temp_buffer and reset it everytime its filled
UTILS_DEF char *temp_sprintf(const char *format, ...);

// ----------------------------------------------------------------------------
//  File Utils
// ----------------------------------------------------------------------------
UTILS_DEF char *read_entire_file(const char *filename);

#ifdef __cplusplus
}
#endif
#endif  // UTILS_H_

#ifdef UTILS_IMPLEMENTATION

static FILE *_log_output_file = NULL;

UTILS_DEF int Log_set_out_file(const char *out_file) {
    if (_log_output_file != NULL) {
        fclose(_log_output_file);  // Close the previous log file if it was open
    }

    if (out_file != NULL && strlen(out_file) > 0) {
        _log_output_file = fopen(out_file, "a");
        if (_log_output_file == NULL) {
            fprintf(stderr, "[ERROR] Can't open output file %s, e: %s\n",
                    out_file, strerror(errno));
            return -1;
        }
    } else {
        _log_output_file = NULL;  // Log to console if no file is provided
    }
    return 0;
}

UTILS_DEF void Log(enum Log_Level level, const char *format, ...) {
    if (level < _base_log_level) return;
    FILE *out = stdout;
    if (level >= Log_Warn) out = stderr;

    if (_log_output_file != NULL) {
        out = _log_output_file;
    }

    switch (level) {
        case Log_Trace:
            fprintf(out, "[TRACE] ");
            break;
        case Log_Debug:
            fprintf(out, "[DEBUG] ");
            break;
        case Log_Info:
            fprintf(out, "[INFO] ");
            break;
        case Log_Warn:
            fprintf(out, "[WARN] ");
            break;
        case Log_Error:
            fprintf(out, "[ERROR] ");
            break;
        case Log_Fatal:
            fprintf(out, "[FATAL] ");
            break;
        case Log_None:
            return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);
    fprintf(out, "\n");
}

UTILS_DEF char *generate_uuid() {
    char v[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char *buf = (char *)malloc(sizeof(char) * (37));
    if (buf == NULL) {
        Log(Log_Error, "generate_uuid: Could not allocate buffer");
        exit(1);
    }

    for (int i = 0; i < 36; ++i) {
        buf[i] = v[rand() % 16];
    }

    buf[8] = '-';
    buf[13] = '-';
    buf[18] = '-';
    buf[23] = '-';

    buf[36] = '\0';

    return buf;
}

UTILS_DEF bool is_number(const char *s) {
    if (s == NULL || *s == '\0') return false;

    if (*s == '-') s++;
    if (*s == '\0') return false;
    while (*s) {
        if (*s < '0' || *s > '9') return false;
        s++;
    }
    return true;
}

int calculate_infix_apply_operator(int a, int b, char op) {
    switch (op) {
        case '+':
            return a + b;
        case '-':
            return a - b;
        case '*':
            return a * b;
        case '/':
            return b != 0 ? a / b : 0;  // avoid division by zero
        case '%':
            return b != 0 ? a % b : 0;
        default:
            return 0;
    }
}

int calculate_infix_precedence(char op) {
    switch (op) {
        case '*':
        case '/':
        case '%':
            return 2;
        case '+':
        case '-':
            return 1;
        default:
            return 0;
    }
}

UTILS_DEF int calculate_infix(const char *expr) {
    int values[100];
    char ops[100];
    int vtop = -1, optop = -1;

    const char *p = expr;
    while (*p) {
        if (isspace(*p)) {
            p++;
            continue;
        }

        // Parse number
        if (isdigit(*p)) {
            int val = 0;
            while (isdigit(*p)) {
                val = val * 10 + (*p - '0');
                p++;
            }
            values[++vtop] = val;
        }
        // Operator
        else {
            while (optop >= 0 && calculate_infix_precedence(ops[optop]) >=
                                     calculate_infix_precedence(*p)) {
                int b = values[vtop--];
                int a = values[vtop--];
                char op = ops[optop--];
                values[++vtop] = calculate_infix_apply_operator(a, b, op);
            }
            ops[++optop] = *p;
            p++;
        }
    }

    while (optop >= 0) {
        int b = values[vtop--];
        int a = values[vtop--];
        char op = ops[optop--];
        values[++vtop] = calculate_infix_apply_operator(a, b, op);
    }

    return values[vtop];
}

UTILS_DEF float triangle_area_float(float x1, float y1, float x2, float y2,
                                    float x3, float y3) {
    return fabsf((x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2)) / 2.0f);
}

UTILS_DEF bool triangle_is_inside(float x1, float y1, float x2, float y2,
                                  float x3, float y3, float x, float y) {
    float A = triangle_area_float(x1, y1, x2, y2, x3, y3);
    float A1 = triangle_area_float(x, y, x2, y2, x3, y3);
    float A2 = triangle_area_float(x1, y1, x, y, x3, y3);
    float A3 = triangle_area_float(x1, y1, x2, y2, x, y);

    // Check if sum of A1, A2 and A3 is same as A
    return (A == A1 + A2 + A3);
}

UTILS_DEF char *strdup(const char *src) {
    if (src == NULL) return NULL;

    size_t len = strlen(src);
    char *dst = malloc(sizeof(char) * (len + 1));
    if (dst == NULL) {
        Log(Log_Error, "strdup: malloc failed");
        return NULL;
    }

    char *ptr = dst;
    while ((*ptr++ = *src++));

    return dst;
}

static char utils_static_temp_buffer[UTILS_MAX_TEMP_SIZE];
static uint32_t utils_static_temp_buffer_pos = 0;

UTILS_DEF char *combine_charp(const char *str1, const char *str2) {
    return temp_sprintf("%s%s", str1, str2);
}

UTILS_DEF char *combine_strings_with_sep_(const char *separator, ...) {
    va_list args;

    // Calculate length
    size_t total_len = 0;
    size_t sep_len = separator ? strlen(separator) : 0;

    va_start(args, separator);
    const char *s = va_arg(args, const char *);
    int count = 0;
    while (s != NULL) {
        total_len += strlen(s);
        s = va_arg(args, const char *);
        count++;
    }
    va_end(args);

    if (count > 0 && separator) {
        total_len += sep_len * (count - 1);
    }

    // Check buffer space
    if (utils_static_temp_buffer_pos + total_len + 1 > UTILS_MAX_TEMP_SIZE) {
        utils_static_temp_buffer_pos = 0;
        if (total_len + 1 > UTILS_MAX_TEMP_SIZE) {
            Log(Log_Error,
                "combine_strings_with_sep_: String too long for buffer");
            return NULL;
        }
    }

    char *start = utils_static_temp_buffer + utils_static_temp_buffer_pos;
    char *current = start;

    va_start(args, separator);
    s = va_arg(args, const char *);
    int i = 0;
    while (s != NULL) {
        size_t len = strlen(s);
        memcpy(current, s, len);
        current += len;

        s = va_arg(args, const char *);
        // Add separator if not the last element
        if (s != NULL && separator) {
            memcpy(current, separator, sep_len);
            current += sep_len;
        }
        i++;
    }
    va_end(args);

    *current = '\0';
    utils_static_temp_buffer_pos += (current - start) + 1;

    return start;
}

UTILS_DEF char *temp_sprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (n < 0) {
        Log(Log_Error, "temp_sprintf: vsnprintf returned neg size");
        return NULL;
    }

    if (n + 1 > UTILS_MAX_TEMP_SIZE) {
        Log(Log_Error, "temp_sprintf: Output too large for buffer");
        return NULL;
    }

    if (utils_static_temp_buffer_pos + n + 1 > UTILS_MAX_TEMP_SIZE) {
        // Log(Log_Info, "temp_sprintf: clearing existing buffer");
        utils_static_temp_buffer_pos = 0;
    }

    va_start(args, format);
    vsnprintf(utils_static_temp_buffer + utils_static_temp_buffer_pos,
              UTILS_MAX_TEMP_SIZE - utils_static_temp_buffer_pos, format, args);
    va_end(args);

    char *ret = utils_static_temp_buffer + utils_static_temp_buffer_pos;
    utils_static_temp_buffer_pos += n + 1;

    return ret;
}

UTILS_DEF char *read_entire_file(const char *filename) {
    if (filename == NULL || strlen(filename) == 0) {
        Log(Log_Warn, "read_entire_file: Invalid file name");
        return NULL;
    }
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        Log(Log_Error,
            temp_sprintf("read_entire_file: Cannot open file %s: %s ", filename,
                         strerror(errno)));
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *contents = (char *)malloc((file_size + 1) * sizeof(char));
    if (contents == NULL) {
        Log(Log_Error,
            temp_sprintf("read_entire_file: Cannot read file %s: malloc failed",
                         filename));
        return NULL;
    }
    fread(contents, file_size, 1, f);
    contents[file_size] = '\0';
    fclose(f);

    return contents;
}

#endif  // UTILS_IMPLEMENTATION
