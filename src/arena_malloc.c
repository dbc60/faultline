/**
 * @file arena_malloc.c
 * @author Douglas Cuthbertson
 * @brief Non-throwing versions of the arena memory allocation functions.
 * @version 0.1
 * @date 2024-12-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/arena.h>
#include <faultline/fl_exception_service.h>        // for fl_invalid_address
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT
#include <faultline/fl_try.h>                      // for FL_CATCH, FL_END_TRY, FL_TRY
#include <faultline/arena_malloc.h> // for arena_free_throw, arena_out_of_me...
#include <stddef.h>                 // for size_t, NULL

void *arena_aligned_alloc(Arena *arena, size_t alignment, size_t size, char const *file,
                          int line) {
    void *mem = NULL;

    // Pre-validate alignment; return NULL for invalid values (non-throw convention)
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    FL_TRY {
        mem = arena_aligned_alloc_throw(arena, alignment, size, file, line);
    }
    FL_CATCH(arena_out_of_memory) {
        mem = NULL;
    }
    FL_END_TRY;

    return mem;
}

void *arena_calloc(Arena *arena, size_t count, size_t size, char const *file, int line) {
    void *mem;
    FL_TRY {
        mem = arena_calloc_throw(arena, count, size, file, line);
    }
    FL_CATCH(arena_out_of_memory) {
        mem = NULL;
    }
    FL_END_TRY;

    return mem;
}

void arena_free(Arena *arena, void *ptr, char const *file, int line) {
    FL_TRY {
        arena_free_throw(arena, ptr, file, line);
    }
    FL_CATCH(fl_invalid_address) {
        // silently handle invalid addresses to avoid corrupting the arena
    }
    FL_END_TRY;
}

void arena_free_pointer(Arena *arena, void **ptr, char const *file, int line) {
    FL_ASSERT_NOT_NULL(ptr);
    FL_TRY {
        arena_free_throw(arena, *ptr, file, line);
    }
    FL_CATCH(fl_invalid_address) {
        // silently handle invalid addresses to avoid corrupting the arena
    }
    FL_END_TRY;

    *ptr = NULL;
}

void *arena_malloc(Arena *arena, size_t bytes, char const *file, int line) {
    void *mem;

    FL_TRY {
        mem = arena_malloc_throw(arena, bytes, file, line);
    }
    FL_CATCH(arena_out_of_memory) {
        mem = NULL;
    }
    FL_END_TRY;

    return mem;
}

/*
 * N.B.: don't get fancy. The only way a caller knows if realloc succeeded is if the
 * returned value does NOT match mem.
 */
void *arena_realloc(Arena *arena, void *mem, size_t size, char const *file, int line) {
    void *p;

    FL_TRY {
        p = arena_realloc_throw(arena, mem, size, file, line);
    }
    FL_CATCH(arena_out_of_memory) {
        // realloc failed, so return the old object unchanged
        p = mem;
    }
    FL_END_TRY;

    return p;
}
