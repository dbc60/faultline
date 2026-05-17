#ifndef FAULTLINE_BUFFER_H_
#define FAULTLINE_BUFFER_H_

/**
 * @file buffer.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Dynamic Buffer Library - Public API
 *
 * This is partially based on the dynamic array implementation in Hanson, David R. C
 * Interfaces and Implementation: Techniques For Creating Reusable Software.
 * Addison-Wesley; 1997. However, this implementation does not support inserting elements
 * at an arbitrary index. It maintains a count of the number of elements in the Buffer so
 * the caller doesn't have to.
 *
 * @version 0.2.0
 * @date 2025-01-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/arena.h>                       // for Arena, ARENA_CALLOC_THROW
#include <faultline/fl_exception_service_assert.h> // for FL_ASSERT_NOT_NULL, FL_ASSE...
#include <faultline/fl_macros.h>                   // for FL_MAYBE_UNUSED
#include <faultline/fl_try.h>                      // for FL_THROW_DETAILS
#include <stdbool.h>                               // for bool, false, true
#include <stddef.h>                                // for size_t, NULL
#include <faultline/fl_exception_service.h>        // for fl_invalid_value
#include <faultline/fl_exception_types.h>          // for FLExceptionReason

#if defined(__cplusplus)
extern "C" {
#endif

extern FLExceptionReason buffer_out_of_memory;

/**
 * @brief the descriptor for a Buffer.
 */
typedef struct Buffer {
    Arena *arena;
    size_t count;        /// the number of elements in the buffer
    size_t element_size; /// the size of each element in bytes
    size_t capacity;     /// the number of elements the buffer can hold
    char  *mem;          /// the block of memory treated as a buffer
    bool   initialized;  /// whether the buffer has been initialized
    bool   created; /// whether the buffer was allocated by new_buffer or an external
                    /// address was used to create it
} Buffer;

/**
 * @brief Define a custom Buffer type
 *
 * @param VT the type of the values to store
 * @param PRE a unique name to use as a prefix for the set function names
 */
#define DEFINE_TYPED_BUFFER(VT, PRE)                                                 \
    typedef Buffer VT##Buffer;                                                       \
                                                                                     \
    static inline FL_MAYBE_UNUSED void init_##PRE##_buffer(VT##Buffer *buf,          \
                                                           Arena      *arena,        \
                                                           size_t      capacity) {   \
        FL_ASSERT_NOT_NULL(buf);                                                     \
        init_buffer((Buffer *)buf, arena, capacity, sizeof(VT));                     \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED VT##Buffer *new_##PRE##_buffer(Arena *arena,       \
                                                                 size_t capacity) {  \
        return (VT##Buffer *)new_buffer(arena, capacity, sizeof(VT));                \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED void destroy_##PRE##_buffer(VT##Buffer *buf) {     \
        destroy_buffer((Buffer *)buf);                                               \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED bool PRE##_buffer_is_initialized(                  \
        VT##Buffer const *buf) {                                                     \
        return buffer_is_initialized((Buffer *)buf);                                 \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED VT *PRE##_buffer_put(VT##Buffer *buf,              \
                                                       VT const   *item) {           \
        return (VT *)buffer_put((Buffer *)buf, (void *)item);                        \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED VT *PRE##_buffer_get(VT##Buffer const *buf,        \
                                                       size_t            index) {    \
        return (VT *)buffer_get((Buffer *)buf, index);                               \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED size_t PRE##_buffer_count(VT##Buffer const *buf) { \
        return buffer_count((Buffer *)buf);                                          \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED void PRE##_buffer_clear(VT##Buffer *buf) {         \
        buffer_clear((Buffer *)buf);                                                 \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED void PRE##_buffer_clear_and_release(               \
        VT##Buffer *buf) {                                                           \
        buffer_clear_and_release((Buffer *)buf);                                     \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED VT *PRE##_buffer_allocate_next_free_slot(          \
        VT##Buffer *buf) {                                                           \
        return (VT *)buffer_allocate_next_free_slot((Buffer *)buf);                  \
    }                                                                                \
                                                                                     \
    static inline FL_MAYBE_UNUSED size_t PRE##_buffer_copy(VT##Buffer       *dst,    \
                                                           VT##Buffer const *src) {  \
        return buffer_copy((Buffer *)dst, (Buffer *)src);                            \
    }

/**
 * @brief initialize a buffer from a contiguous memory block.
 *
 * It is a checked runtime error for:
 *  - buffer to be NULL.
 *  - size to be non-positive.
 *  - length to be nonzero and mem to be NULL.
 *  - length to be non-positive and mem to be non-NULL.
 *
 * If capacity > 0, then mem must be non-NULL. If capacity is zero, the mem must be NULL.
 *
 * It is an unchecked runtime error to initialize a buffer with a capacity > 0 and
 * insufficient memory to store (element_size * capacity) elements.
 *
 * It is an unchecked runtime error to initialize an Buffer by means other than calling
 * buffer_init, though an Buffer can be allocated and initialized by calling
 * new_buffer().
 *
 * @param buf the Buffer to initialize.
 * @param capacity the number of elements in the buffer.
 * @param element_size the size of each element in bytes including any padding needed for
 * alignment.
 * @param mem the block of memory to treat as a buffer.
 */
static inline void buffer_initialize(Buffer *buf, Arena *arena, size_t capacity,
                                     size_t element_size, void *mem, bool created) {
    FL_ASSERT_NOT_NULL(buf);
    FL_ASSERT_TRUE(element_size > 0);
    buf->arena        = arena;
    buf->count        = 0;
    buf->capacity     = capacity;
    buf->element_size = element_size;
    buf->mem          = mem;
    buf->initialized  = true;
    buf->created      = created;
}

/**
 * @brief Initialize a block of memory pointed to by buf as a Buffer
 *
 * @param buf the Buffer to initialize
 * @param arena the Arena to use for memory allocations
 * @param capacity the initial capacity of the buffer
 * @param element_size the size of each element in bytes
 */
static inline void init_buffer(Buffer *buf, Arena *arena, size_t capacity,
                               size_t element_size) {
    void *mem = NULL;
    if (capacity > 0) {
        mem = ARENA_CALLOC_THROW(arena, capacity, element_size);
    }
    buffer_initialize(buf, arena, capacity, element_size, mem, false);
}

/**
 * @brief make new Buffer of elements of element_size bytes with the given capacity.
 *
 * The bounds of the buffer are zero through capacity-1 unless capacity is zero, in which
 * case the buffer has no elements. Each element occupies element_size bytes, and
 * element_size must include any padding required for alignment. The elements, if any,
 * are initialized to zero.
 *
 * Arrays are created in two steps. First, new_buffer allocates an Buffer object, then it
 * allocates the block of memory on which the Buffer operates.
 *
 * It is a checked runtime error for element_size to be zero.
 *
 * @param capacity the number of elements the buffer can hold
 * @param element_size the size in bytes of each element
 * @return the block of memory treated as a buffer
 * @throw buffer_out_of_memory, fl_invalid_value
 */
Buffer *new_buffer(Arena *arena, size_t capacity, size_t element_size);

/**
 * @brief release the memory resources acquired through new_buffer and sets *buf to NULL.
 *
 * It is a checked runtime error for buf or *buf to be NULL.
 *
 * @param buf the address of a Buffer.
 * @throw fl_invalid_value
 */
void destroy_buffer(Buffer *buf);

static inline bool buffer_is_initialized(Buffer const *buf) {
    FL_ASSERT_NOT_NULL(buf);
    return buf->initialized;
}

/**
 * @brief copy a new element to the end of the buffer and return its address in the
 * buffer.
 *
 * It is an unchecked runtime error to call buffer_get and then change the size of
 * buffer via buffer_resize before dereferencing the pointer returned by buffer_get.
 * It is also an unchecked runtime error for the storage beginning at elem to overlap in
 * any way with the storage of the buffer.
 *
 * @param buf the address of the Buffer
 * @param elem the address of the new element to copy to the end of the buffer
 * @return the address of the ith element
 * @throw fl_invalid_value
 */
void *buffer_put(Buffer *buf, void const *elem);

void *buffer_allocate_next_free_slot(Buffer *buf);

/**
 * @brief get the address of element at index i.
 *
 * @param buf the address of a Buffer
 * @param i the index of the element to retrieve
 * @return the address of the ith element
 * @throw fl_invalid_value if i is greater than or equal to the number of elements in
 * the buffer
 */
static inline void *buffer_get(Buffer const *buf, size_t i) {
    FL_ASSERT_NOT_NULL(buf);
    if (i >= buf->count) {
        FL_THROW_DETAILS(fl_invalid_value, "buffer index %zu out of bounds (count: %zu)",
                         i, buf->count);
    }

    return buf->mem + i * buf->element_size;
}

/**
 * @brief get the number of elements in the buffer.
 *
 * @param buf
 * @return the number of elements in the buffer
 */
static inline size_t buffer_count(Buffer const *buf) {
    FL_ASSERT_NOT_NULL(buf);
    return buf->count;
}

/**
 * @brief empty the buffer without deallocating memory.
 *
 * @param buf the buffer to empty
 */
static inline void buffer_clear(Buffer *buf) {
    FL_ASSERT_NOT_NULL(buf);
    buf->count = 0;
}

/**
 * @brief empty the buffer and release its memory, but don't change the size of the
 * elements it contains.
 *
 * If the buffer is already empty, this function does nothing.
 *
 * It is an unchecked runtime error to call this function on a NULL pointer.
 *
 * @param buf the address of a Buffer
 */
static inline void buffer_clear_and_release(Buffer *buf) {
    FL_ASSERT_NOT_NULL(buf);
    if (buf->mem != NULL) {
        ARENA_FREE_THROW(buf->arena, buf->mem);
        buf->mem = NULL;
    }
    buf->count    = 0;
    buf->capacity = 0;
}

/**
 * @brief copy all of the elements from src to dst
 *
 * @param dst the destination buffer
 * @param src the source buffer
 * @return size_t the number of elements copied
 */
size_t buffer_copy(Buffer *dst, Buffer const *src);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_BUFFER_H_
