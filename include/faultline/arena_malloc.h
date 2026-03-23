#ifndef ARENA_MALLOC_H_
#define ARENA_MALLOC_H_

/**
 * @file arena_malloc.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.2.0
 * @date 2024-12-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_exception_types.h> // for FLExceptionReason
#include <stddef.h>                       // for size_t
#include <faultline/arena.h>              // for Arena

#if defined(__cplusplus)
extern "C" {
#endif

extern FLExceptionReason arena_out_of_memory;

/////////////////////////////////
// Memory Management Functions //
/////////////////////////////////

typedef struct Arena Arena;

/**
 * @brief Non-throwing version of arena_aligned_alloc_throw.
 *
 * Returns NULL if the allocation fails or if alignment is invalid.
 *
 * @param alignment the required alignment in bytes (must be a power of 2)
 * @param size the number of bytes to allocate
 * @param file
 * @param line
 * @return a pointer to size bytes of memory aligned to alignment bytes, or NULL
 */
void *arena_aligned_alloc(Arena *arena, size_t alignment, size_t size, char const *file,
                          int line);

/**
 * @brief allocate a block of memory consisting of count elements, each of size bytes.
 *
 * @param count the number of elements.
 * @param bytes the size of each element in bytes.
 * @param file
 * @param line
 * @return a block of memory or NULL if the request cannot be satisfied.
 */
void *arena_calloc(Arena *arena, size_t count, size_t bytes, char const *file, int line);

/**
 * @brief free a block of memory.
 *
 * @param mp the address of the memory to free.
 */
void arena_free(Arena *arena, void *mp, char const *file, int line);

/**
 * @brief free memory pointed to by *mp and set *mp to NULL.
 *
 * @param mp the memory address of the pointer to the memory to free.
 */
void arena_free_pointer(Arena *arena, void **mp, char const *file, int line);

/**
 * @brief allocate a block of memory that is at least request number of bytes in size.
 *
 * @param bytes the number of bytes requested
 * @param file
 * @param line
 * @return the address of a block of memory that satisfies the request
 * @throw arena_out_of_memory exception.
 */
void *arena_malloc(Arena *arena, size_t bytes, char const *file, int line);

/**
 * @brief The realloc function deallocates the old object pointed to by mem and returns a
 * pointer to a new object that has the size specified by size. The contents of the new
 * object shall be the same as that of the old object prior to deallocation, up to the
 * lesser of the new and old sizes. Any bytes in the new object beyond the size of the
 * old object have indeterminate values.
 *
 * If mem is a null pointer, the realloc function behaves like the malloc function for
 * the specified size. Otherwise, if mem does not match a pointer earlier returned by the
 * calloc, malloc, or realloc function, or if the space has been deallocated by a call to
 * the free or realloc function, the behavior is undefined. If memory for the new object
 * cannot be allocated, the old object is not deallocated and its value is unchanged.
 *
 * @param mem
 * @param size
 * @param file
 * @param line
 * @return a block of memory
 */
void *arena_realloc(Arena *arena, void *mem, size_t size, char const *file, int line);

#if defined(__cplusplus)
}
#endif

#endif // ARENA_MALLOC_H_
