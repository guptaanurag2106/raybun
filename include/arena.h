#ifndef ARENA_H_
#define ARENA_H_

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ARENA_DEF
#ifdef ARENA_IMPLEMENTATION
#define ARENA_DEF
#else
#define ARENA_DEF extern
#endif
#endif

#ifndef ARENA_BACKEND_MALLOC
#define ARENA_BACKEND_MALLOC malloc
#endif

#ifndef ARENA_BACKEND_FREE
#define ARENA_BACKEND_FREE free
#endif

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    uint8_t *current;
} Arena;

ARENA_DEF int arena_create(Arena *arena, size_t size);

ARENA_DEF void arena_destroy(Arena *a);

ARENA_DEF void arena_clear(Arena *a);

ARENA_DEF void *arena_alloc(Arena *a, size_t size);

ARENA_DEF void *arena_alloc_zeroed(Arena *a, size_t size);

ARENA_DEF void *arena_alloc_aligned(Arena *a, size_t size, size_t align);

ARENA_DEF void *arena_alloc_aligned_zeroed(Arena *a, size_t size, size_t align);

#define arena_alloc_struct(arena, type) \
    (type *)arena_alloc_aligned(arena, sizeof(type), _Alignof(type))

#define arena_alloc_struct_zeroed(arena, type) \
    (type *)arena_alloc_aligned_zeroed(arena, sizeof(type), _Alignof(type))

#define arena_alloc_array(arena, type, count) \
    (type *)arena_alloc_aligned(arena, sizeof(type) * (count), _Alignof(type))

#define arena_alloc_array_zeroed(arena, type, count)                  \
    (type *)arena_alloc_aligned_zeroed(arena, sizeof(type) * (count), \
                                       _Alignof(type))

ARENA_DEF void *arena_realloc(Arena *a, const void *ptr, size_t old_size,
                              size_t new_size);

ARENA_DEF void *arena_realloc_aligned(Arena *a, const void *ptr,
                                      size_t old_size, size_t new_size,
                                      size_t align);

#define arena_realloc_struct(arena, ptr, type)                            \
    (type *)arena_realloc_aligned(arena, ptr, sizeof(type), sizeof(type), \
                                  _Alignof(type))

#define arena_realloc_array(arena, ptr, type, count)                  \
    (type *)arena_realloc_aligned(arena, ptr, sizeof(type) * (count), \
                                  sizeof(type) * (count), _Alignof(type))

typedef uint8_t *ArenaCheckpoint;

ARENA_DEF ArenaCheckpoint arena_get_checkpoint(Arena *a);

ARENA_DEF void *arena_get_ptr(ArenaCheckpoint cp);

ARENA_DEF void arena_rewind(Arena *a, ArenaCheckpoint cp);

ARENA_DEF char *arena_sprintf(Arena *a, const char *format, ...);

ARENA_DEF char *arena_combine_charp(Arena *a, const char *str1,
                                    const char *str2);

ARENA_DEF char *arena_combine_strings_with_sep_(Arena *a, const char *separator,
                                                ...);

#define ARENA_COMBINE(arena, separator, ...) \
    arena_combine_strings_with_sep_((arena), (separator), __VA_ARGS__, NULL)

ARENA_DEF char *arena_read_entire_file(Arena *arena, const char *filename);

#define arena_vec__grow(arena, v, new_cap)                                     \
    do {                                                                       \
        size_t _align = _Alignof(*(v)->items);                                 \
        size_t _old_sz = (v)->size * sizeof(*(v)->items);                      \
        size_t _new_sz = (new_cap) * sizeof(*(v)->items);                      \
        void *_new_items = arena_realloc_aligned((arena), (v)->items, _old_sz, \
                                                 _new_sz, _align);             \
        assert(_new_items != NULL && "vec_push/reserve: realloc failed");      \
        if (_new_items == NULL) break;                                         \
        (v)->items = (__typeof__((v)->items))_new_items;                       \
        (v)->capacity = (new_cap);                                             \
    } while (0)

#define arena_vec_reserve(arena, v, n)      \
    do {                                    \
        arena_vec__grow((arena), (v), (n)); \
    } while (0)

// Returns 0 on success, -1 on OOM.
#define arena_vec_push(arena, v, value)                                 \
    do {                                                                \
        if ((v)->size >= (v)->capacity) {                               \
            size_t _old_cap = (v)->capacity;                            \
            size_t _new_cap = (_old_cap == 0) ? 4 : (_old_cap * 1.618); \
            arena_vec__grow((arena), (v), _new_cap);                    \
        }                                                               \
        (v)->items[(v)->size++] = (value);                              \
    } while (0)

#ifdef __cplusplus
}
#endif
#endif  // ARENA_H_

#ifdef ARENA_IMPLEMENTATION

ARENA_DEF int arena_create(Arena *arena, size_t size) {
    arena->buffer = (uint8_t *)(ARENA_BACKEND_MALLOC(size));
    if (arena->buffer == NULL) {
        fprintf(stderr, "arena_create: Could not malloc %zu, exiting\n", size);
        return -1;
    }
    arena->capacity = size;
    arena->current = arena->buffer + size;
    return 0;
}

ARENA_DEF void arena_destroy(Arena *a) {
    if (a == NULL) return;
    ARENA_BACKEND_FREE(a->buffer);
    a->buffer = NULL;
    a->capacity = 0;
    a->current = NULL;
}

ARENA_DEF void arena_clear(Arena *a) { a->current = a->buffer + a->capacity; }

ARENA_DEF void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
    assert(align && (align & (align - 1)) == 0);

    uintptr_t cur = (uintptr_t)a->current;
    uintptr_t base = (uintptr_t)a->buffer;

    if (size > (size_t)(cur - base)) {
        fprintf(stderr, "arena_alloc: Could not alloc, not enough space\n");
        return NULL;  // not enough space
    }

    uintptr_t ptr = cur - size;
    uintptr_t aligned = ptr & ~(align - 1);

    if (aligned < base) return NULL;

    a->current = (uint8_t *)aligned;

    return a->current;
}

ARENA_DEF void *arena_alloc_aligned_zeroed(Arena *a, size_t size,
                                           size_t align) {
    void *ptr = arena_alloc_aligned(a, size, align);
    if (ptr == NULL) return NULL;
    memset(a->current, 0, size);
    return a->current;
}

ARENA_DEF void *arena_alloc(Arena *a, size_t size) {
    return arena_alloc_aligned(a, size, _Alignof(max_align_t));
}

ARENA_DEF void *arena_alloc_zeroed(Arena *a, size_t size) {
    return arena_alloc_aligned_zeroed(a, size, _Alignof(max_align_t));
}

// TODO:see if still works if the ptr is the last allocated thing (can we reuse
// the space somehow)
ARENA_DEF void *arena_realloc_aligned(Arena *a, const void *ptr,
                                      size_t old_size, size_t new_size,
                                      size_t align) {
    if (new_size <= old_size) return (void *)ptr;
    void *new_ptr = arena_alloc_aligned(a, new_size, align);
    if (new_ptr == NULL) return NULL;
    if (old_size && ptr) memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

ARENA_DEF void *arena_realloc(Arena *a, const void *ptr, size_t old_size,
                              size_t new_size) {
    return arena_realloc_aligned(a, ptr, old_size, new_size,
                                 _Alignof(max_align_t));
}

ARENA_DEF ArenaCheckpoint arena_get_checkpoint(Arena *a) { return a->current; }

ARENA_DEF void *arena_get_ptr(ArenaCheckpoint cp) { return (void *)cp; }

ARENA_DEF void arena_rewind(Arena *a, ArenaCheckpoint cp) {
    assert(cp >= a->buffer && cp <= a->buffer + a->capacity);
    a->current = cp;
}

ARENA_DEF char *arena_sprintf(Arena *a, const char *format, ...) {
    va_list args;
    va_start(args, format);

    va_list args2;
    va_copy(args2, args);
    int n = vsnprintf(NULL, 0, format, args2);
    va_end(args2);

    if (n < 0) {
        va_end(args);
        return NULL;
    }

    char *buf = (char *)arena_alloc_aligned(a, (size_t)n + 1, _Alignof(char));
    if (buf == NULL) {
        va_end(args);
        return NULL;
    }

    vsnprintf(buf, (size_t)n + 1, format, args);
    va_end(args);

    return buf;
}

ARENA_DEF char *arena_combine_charp(Arena *a, const char *str1,
                                    const char *str2) {
    return arena_sprintf(a, "%s%s", str1, str2);
}

ARENA_DEF char *arena_combine_strings_with_sep_(Arena *a, const char *separator,
                                                ...) {
    va_list args;

    size_t total_len = 0;
    size_t sep_len = separator ? strlen(separator) : 0;
    size_t count = 0;

    va_start(args, separator);
    const char *s = va_arg(args, const char *);
    while (s != NULL) {
        total_len += strlen(s);
        count++;
        s = va_arg(args, const char *);
    }
    va_end(args);

    if (count > 0 && separator) total_len += sep_len * (count - 1);

    char *out = (char *)arena_alloc_aligned(a, total_len + 1, _Alignof(char));
    if (out == NULL) return NULL;

    char *cur = out;
    va_start(args, separator);
    s = va_arg(args, const char *);
    while (s != NULL) {
        size_t len = strlen(s);
        memcpy(cur, s, len);
        cur += len;

        s = va_arg(args, const char *);
        if (s != NULL && separator) {
            memcpy(cur, separator, sep_len);
            cur += sep_len;
        }
    }
    va_end(args);

    *cur = '\0';
    return out;
}

ARENA_DEF char *arena_read_entire_file(Arena *arena, const char *filename) {
    if (filename == NULL || strlen(filename) == 0) {
        fprintf(stderr, "read_entire_file: Invalid file name");
        return NULL;
    }
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        fprintf(stderr, "read_entire_file: Cannot open file %s: %s ", filename,
                strerror(errno));
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) < 0) {
        fprintf(stderr, "read_entire_file: Cannot read file %s: %s ", filename,
                strerror(errno));
        return NULL;
    }
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *contents = (char *)arena_alloc_aligned(
        arena, (file_size + 1) * sizeof(char), _Alignof(char));
    if (contents == NULL) {
        fprintf(stderr, "read_entire_file: Cannot read file %s: malloc failed",
                filename);
        return NULL;
    }
    size_t read = fread(contents, 1, file_size, f);
    if (read != (size_t)file_size) {
        if (ferror(f)) {
            fprintf(stderr,
                    "read_entire_file: Error while reading %s: "
                    "read %zu bytes out of %zu, %s",
                    filename, read, file_size, strerror(errno));
            clearerr(f);
        } else {
            fprintf(stderr,
                    "read_entire_file: Error while reading %s: "
                    "read %zu bytes out of %ld",
                    filename, read, file_size);
        }
    }
    if (ferror(f)) {
        fprintf(stderr, "read_entire_file: Error while reading %s:%s", filename,
                strerror(errno));
    }
    contents[file_size] = '\0';
    fclose(f);

    return contents;
}

#endif
