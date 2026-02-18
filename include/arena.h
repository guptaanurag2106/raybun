#ifndef ARENA_H_
#define ARENA_H_

#include <assert.h>
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

ARENA_DEF Arena arena_create(size_t size);

ARENA_DEF void arena_destroy(Arena *a);

ARENA_DEF void arena_clear(Arena *a);

ARENA_DEF void *arena_alloc(Arena *a, size_t size);

ARENA_DEF void *arena_alloc_zeroed(Arena *a, size_t size);

ARENA_DEF void *arena_alloc_aligned(Arena *a, size_t size, size_t align);

ARENA_DEF void *arena_alloc_aligned_zeroed(Arena *a, size_t size, size_t align);

#define ARENA_PUSH_STRUCT(arena, type) \
    (type *)arena_alloc_aligned(arena, sizeof(type), _Alignof(type))

#define ARENA_PUSH_STRUCT_ZEROED(arena, type) \
    (type *)arena_alloc_aligned_zeroed(arena, sizeof(type), _Alignof(type))

#define ARENA_PUSH_ARRAY(arena, type, count) \
    (type *)arena_alloc_aligned(arena, sizeof(type) * (count), _Alignof(type))

#define ARENA_PUSH_ARRAY_ZEROED(arena, type, count)                   \
    (type *)arena_alloc_aligned_zeroed(arena, sizeof(type) * (count), \
                                       _Alignof(type))

typedef uint8_t *ArenaCheckpoint;

ARENA_DEF ArenaCheckpoint arena_get_checkpoint(Arena *a);

ARENA_DEF void *arena_get_ptr(Arena *a, ArenaCheckpoint cp);

ARENA_DEF void arena_rewind(Arena *a, ArenaCheckpoint cp);

#ifdef __cplusplus
}
#endif
#endif  // ARENA_H_

#ifdef ARENA_IMPLEMENTATION
ARENA_DEF Arena arena_create(size_t size) {
    Arena a;
    a.buffer = ARENA_BACKEND_MALLOC(size);
    if (a.buffer == NULL) {
        fprintf(stderr, "arena_create: Could not malloc %zu, exitting\n", size);
        exit(1);
    }
    a.capacity = size;
    a.current = a.buffer + size;
    return a;
}

ARENA_DEF void arena_destroy(Arena *a) {
    ARENA_BACKEND_FREE(a->buffer);
    a->buffer = NULL;
    a->capacity = 0;
    a->current = NULL;
}

ARENA_DEF void arena_clear(Arena *a) { a->current = a->buffer + a->capacity; }

ARENA_DEF void *arena_alloc(Arena *a, size_t size) {
    size_t available = (size_t)(a->current - a->buffer);
    if (size > available) return NULL;

    a->current -= size;
    return a->current;
}

ARENA_DEF void *arena_alloc_zeroed(Arena *a, size_t size) {
    arena_alloc(a, size);
    memset(a->current, 0, size);
    return a->current;
}

ARENA_DEF void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
    assert((align & (align - 1)) == 0);
    assert(align > 0);

    size_t available = (size_t)(a->current - a->buffer);
    if (size > available) return NULL;

    uintptr_t ptr = (uintptr_t)(a->current - size);
    uintptr_t aligned = ptr & ~(align - 1);

    if (aligned < (uintptr_t)a->buffer) return NULL;

    a->current = (uint8_t *)aligned;
    return a->current;
}

ARENA_DEF void *arena_alloc_aligned_zeroed(Arena *a, size_t size,
                                           size_t align) {
    arena_alloc_aligned(a, size, align);
    memset(a->current, 0, size);
    return a->current;
}

ARENA_DEF ArenaCheckpoint arena_get_checkpoint(Arena *a) { return a->current; }

ARENA_DEF void *arena_get_ptr(Arena *a, ArenaCheckpoint cp) {
    return (void *)cp;
}

ARENA_DEF void arena_rewind(Arena *a, ArenaCheckpoint cp) {
    assert(cp >= a->buffer && cp <= a->buffer + a->capacity);
    a->current = cp;
}

#endif
