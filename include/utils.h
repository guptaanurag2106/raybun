#ifndef UTILS_H
#define UTILS_H

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _cplusplus
extern "C" {
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
static FILE *_log_output_file = NULL;

static inline void Log_set_level(enum Log_Level level) {
    _base_log_level = level;
}

static inline int Log_set_out_file(const char *out_file) {
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

void Log(enum Log_Level level, const char *message);
// ----------------------------------------------------------------------------
//  General Utils
// ----------------------------------------------------------------------------
#ifndef DEBUG
#define ASSERT(x) ((void)0)
#else
#define ASSERT(x)                                                           \
    do {                                                                    \
        if (!(x)) {                                                         \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #x, __FILE__, \
                    __LINE__);                                              \
            abort();                                                        \
        }                                                                   \
    } while (0)
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

static inline char *shift(int *argc, char ***argv) {
    ASSERT(*argc > 0);
    (*argc)--;
    return *((*argv)++);
}

char *generate_uuid();

// ----------------------------------------------------------------------------
//  Math Utils
// ----------------------------------------------------------------------------
#define PI 3.14159265359f
#define PI_2 (PI / 2)
#define PI_3_4 (3.0f * PI / 4)
#define PI_2_3 (2.0f * PI / 3)

#define DEG2RAD(_d) ((_d) * (PI / 180.0f))
#define RAD2DEG(_r) ((_r) * (180.0f / PI))

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

static inline bool is_number(const char *s) {
    if (s == NULL || *s == '\0') return false;

    if (*s == '-') s++;
    if (*s == '\0') return false;
    while (*s) {
        if (*s < '0' || *s > '9') return false;
        s++;
    }
    return true;
}

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

int calculate_infix(const char *expr);  // 34+5*10+3 -> 88 just +-*/%

static inline float triangle_area_float(float x1, float y1, float x2, float y2,
                                        float x3, float y3) {
    return fabs((x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2)) / 2.0);
}

static inline bool triangle_is_inside(float x1, float y1, float x2, float y2,
                                      float x3, float y3, float x, float y) {
    float A = triangle_area_float(x1, y1, x2, y2, x3, y3);
    float A1 = triangle_area_float(x, y, x2, y2, x3, y3);
    float A2 = triangle_area_float(x1, y1, x, y, x3, y3);
    float A3 = triangle_area_float(x1, y1, x2, y2, x, y);

    // Check if sum of A1, A2 and A3 is same as A
    return (A == A1 + A2 + A3);
}

// ----------------------------------------------------------------------------
//  String Utils
// ----------------------------------------------------------------------------
#define UTILS_MAX_TEMP_SIZE 1024
// Will malloc combined, free it yourself
void combine_charp(const char *str1, const char *str2, char **combined);
// Will use the utils_static_payload_buffer and reset it everytime
#define COMBINE(separator, ...) \
    combine_strings_with_sep_(separator, __VA_ARGS__, NULL)
// Will use the utils_static_payload_buffer and reset it everytime, last
// va_arg should be NULL
char *combine_strings_with_sep_(const char *separator, ...);
// Will use the utils_static_temp_buffer and reset it everytime
char *temp_sprintf(const char *format, ...);

// ----------------------------------------------------------------------------
//  File Utils
// ----------------------------------------------------------------------------
char *read_entire_file(const char *filename);

// ----------------------------------------------------------------------------
//  Vector Utils
// ----------------------------------------------------------------------------
typedef struct {
    int *data;
    size_t size;
    size_t capacity;
} Ivector;

Ivector *init_Ivector(size_t init_cap);
void resize_Ivector(Ivector *vector, size_t new_capacity);
void push_back_Ivector(Ivector *vector, int val);
int pop_back_Ivector(Ivector *vector);
int get_Ivector(Ivector *vector, size_t pos);
void reset_Ivector(Ivector *vector);

#ifdef _cplusplus
};
#endif
#endif  // UTILS_H

#ifdef UTILS_IMPLEMENTATION

void Log(enum Log_Level level, const char *message) {
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
    fprintf(out, "%s\n", message);
}

char *generate_uuid() {
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
int calculate_infix(const char *expr) {
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

static char utils_static_payload_buffer[UTILS_MAX_TEMP_SIZE];
static char utils_static_temp_buffer[UTILS_MAX_TEMP_SIZE];

void combine_charp(const char *str1, const char *str2, char **combined) {
    size_t length = strlen(str1) + strlen(str2) + 1;
    *combined = (char *)malloc(length);

    if (*combined == NULL) {
        Log(Log_Error, "combine_charp: Could not allocate buffer");
        return;
    }

    strcpy(*combined, str1);
    strcat(*combined, str2);
}

char *combine_strings_with_sep_(const char *separator, ...) {
    va_list args;
    va_start(args, separator);
    const char *str = va_arg(args, const char *);
    int count = 0;
    while (str != NULL) {
        str = va_arg(args, const char *);
        count++;
    }
    va_end(args);

    va_start(args, separator);
    size_t offset = 0;
    str = va_arg(args, const char *);
    for (int i = 0; i < count; i++) {
        size_t len = strlen(str);
        memcpy(utils_static_payload_buffer + offset, str, len);
        offset += len;
        if (i < count - 1) {
            memcpy(utils_static_payload_buffer + offset, separator,
                   strlen(separator));
            offset += strlen(separator);
        }

        str = va_arg(args, const char *);
    }
    va_end(args);
    utils_static_payload_buffer[offset] = '\0';
    return utils_static_payload_buffer;
}

char *temp_sprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (n < 0) {
        Log(Log_Error, "temp_sprintf: vsnprintf returned neg size");
        return NULL;
    }
    if (n >= UTILS_MAX_TEMP_SIZE) {
        Log(Log_Error,
            "temp_sprintf: vsnprintf returned size greater than "
            "UTILS_MAX_TEMP_SIZE");
        return NULL;
    }

    va_start(args, format);
    vsnprintf(utils_static_temp_buffer, UTILS_MAX_TEMP_SIZE, format, args);
    va_end(args);

    return utils_static_temp_buffer;
}

char *read_entire_file(const char *filename) {
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

Ivector *init_Ivector(size_t init_cap) {
    Ivector *vector = (Ivector *)malloc(sizeof(Ivector));
    if (vector == NULL) {
        Log(Log_Error, "init_Ivector: Could not allocate Ivector");
        exit(1);
    }

    vector->data = (int *)malloc(sizeof(int) * init_cap);
    vector->size = 0;
    vector->capacity = init_cap;

    return vector;
}

void resize_Ivector(Ivector *vector, size_t new_capacity) {
    vector->data = (int *)realloc(vector->data, sizeof(int) * new_capacity);

    vector->capacity = new_capacity;
}

void push_back_Ivector(Ivector *vector, int val) {
    if (vector->size >= vector->capacity) {
        resize_Ivector(vector, vector->size * 2);
    }
    vector->data[vector->size++] = val;
}

int pop_back_Ivector(Ivector *vector) {
    if (vector->size == 0) {
        Log(Log_Error, "Ivector_pop_back: cannot pop from empty vector");
        exit(1);
    }

    int val = vector->data[vector->size - 1];
    vector->size--;
    return val;
}

int get_Ivector(Ivector *vector, size_t pos) {
    if (pos >= vector->size) {
        Log(Log_Error,
            temp_sprintf("Ivector_get: index %zu out of bounds (%zu)", pos,
                         vector->size));
        exit(1);
    }
    return vector->data[pos];
}

void reset_Ivector(Ivector *vector) {
    free(vector->data);
    vector->size = 0;
    vector->capacity = 0;
}

#endif  // UTILS_IMPLEMENTATION
