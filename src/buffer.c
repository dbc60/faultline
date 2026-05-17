/**
 * @file buffer.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-01-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_exception_service_assert.h> // for assert, FL_ASSERT_DETAILS
#include <faultline/arena.h>                       // for ARENA_FREE_THROW, ARENA_MAL...
#include <faultline/buffer.h>                      // for Buffer, buffer_initialize
#include <stdbool.h>                               // for true
#include <stdint.h>                                // for SIZE_MAX
#include <string.h>                                // for NULL, size_t, memcpy

// 12 is an arbitrary number of elements to increase the capacity of a Buffer.
#define INCREASE_CAPACITY_BY 12

/**
 * @brief increase the capacity of a Buffer by count elements.
 *
 * @param buf the Buffer to enlarge.
 * @param count the number of elements to add to the buf.
 */
static void increase_capacity(Buffer *buf, size_t count) {
    FL_ASSERT_NOT_NULL(buf);
    FL_ASSERT_GT_SIZE_T(count, (size_t)0);
    FL_ASSERT_DETAILS(
        buf->capacity <= SIZE_MAX - count
            && (buf->capacity + count) <= SIZE_MAX / buf->element_size,
        "buffer capacity overflow: count: %zu, element size: %zu, capacity: %zu", count,
        buf->element_size, buf->capacity);

    if (buf->capacity == 0) {
        FL_ASSERT_NULL(buf->mem);
        buf->mem      = ARENA_MALLOC_THROW(buf->arena, count * buf->element_size);
        buf->capacity = count;
    } else {
        FL_ASSERT_NOT_NULL(buf->mem);
        buf->mem = ARENA_REALLOC_THROW(buf->arena, buf->mem,
                                       (buf->capacity + count) * buf->element_size);
        buf->capacity += count;
    }
}

Buffer *new_buffer(Arena *arena, size_t capacity, size_t element_size) {
    FL_ASSERT_DETAILS(element_size != 0, "element_size is zero");

    Buffer *buf = ARENA_MALLOC_THROW(arena, sizeof *buf);
    void   *mem = NULL;
    if (capacity > 0) {
        mem = ARENA_CALLOC_THROW(arena, capacity, element_size);
    }
    buffer_initialize(buf, arena, capacity, element_size, mem, true);

    return buf;
}

void destroy_buffer(Buffer *buf) {
    FL_ASSERT_NOT_NULL(buf);

    if (buf->mem != NULL) {
        ARENA_FREE_THROW(buf->arena, buf->mem);
    }

    if (buf->created) {
        ARENA_FREE_THROW(buf->arena, buf);
    }
}

/**
 * @brief insert an element into the buffer at the next available index.
 *
 * @param buf
 * @param elem
 * @return void*
 */
void *buffer_put(Buffer *buf, void const *elem) {
    FL_ASSERT_NOT_NULL(buf);
    FL_ASSERT_NOT_NULL(elem);

    // If the buf is full, increase its capacity.
    if (buf->count == buf->capacity) {
        increase_capacity(buf, INCREASE_CAPACITY_BY);
    }

    void *p = buf->mem + buf->count * buf->element_size;
    memcpy(p, elem, buf->element_size);
    buf->count++;
    return p;
}

void *buffer_allocate_next_free_slot(Buffer *buf) {
    FL_ASSERT_NOT_NULL(buf);

    // If the buf is full, increase its capacity.
    if (buf->count == buf->capacity) {
        increase_capacity(buf, INCREASE_CAPACITY_BY);
    }

    void *p = buf->mem + buf->count * buf->element_size;
    buf->count++;
    return p;
}

size_t buffer_copy(Buffer *dst, Buffer const *src) {
    FL_ASSERT_NOT_NULL(dst);
    FL_ASSERT_NOT_NULL(src);

    FL_ASSERT_DETAILS(dst->element_size == src->element_size,
                      "Different element sizes: source %zu, destination %zu",
                      src->element_size, dst->element_size);

    size_t sz = dst->element_size;

    // Ensure dst has enough space
    if (dst->capacity - dst->count < src->count) {
        increase_capacity(dst, src->count - (dst->capacity - dst->count));
    }

    if (src->count > 0) {
        memcpy(dst->mem + dst->count * sz, src->mem, src->count * sz);
    }

    dst->count += src->count;
    return src->count;
}
