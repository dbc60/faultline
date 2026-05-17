#ifndef CHUNK_H_
#define CHUNK_H_

/**
 * @file chunk.h
 * @author Douglas Cuthbertson
 * @brief Memory chunk types and operations for the arena allocator.
 * @version 0.1
 * @date 2025-05-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "bits.h"                // ALIGN_UP, ALIGN_DOWN
#include <faultline/dlist.h>     // DList
#include <faultline/fl_macros.h> // FL_CONTAINER_OF

#include <faultline/fl_try.h>                      // FL_THROW
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT* and fl_unexpected_failure declaration
#include <faultline/size.h>                        // SIZE_T_SIZE, MAX_SIZE_T

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // uintptr_t

/**
 * @brief The P (CHUNK_PREVIOUS_INUSE_FLAG) bit is stored in the unused lowest-order bit
 * of the size field in a chunk. It is set (1) when the previous adjacent chunk is in
 * use.
 *
 * If the previous chunk is in use, all of its data, except the header, is considered
 * payload. When it's not in use, the previous chunk's size will be stored in its last
 * sizeof(size_t) bytes and the P bit in the current chunk will be unset (0).
 */
#define CHUNK_PREVIOUS_INUSE_FLAG 0x1

/**
 * @brief The C (CHUNK_CURRENT_INUSE_FLAG) bit is stored in the unused second-lowest bit
 * of the chunk's size field to redundantly record whether the current chunk is in use.
 * This redundancy enables usage checks within free and realloc, and reduces indirection
 * when freeing and consolidating chunks.
 *
 * Each freshly allocated chunk must have both C and P bits set. That is, each allocated
 * chunk borders either a previously allocated and still in-use chunk, or the base of its
 * memory arena. This is ensured by making all allocations from the 'lowest' address
 * within any found chunk. Furthermore, no free chunk physically borders another one, so
 * each free chunk is known to be preceded and followed by either in-use chunks or one of
 * the ends of memory.
 */
#define CHUNK_CURRENT_INUSE_FLAG 0x2
#define CHUNK_INUSE_FLAGS        (CHUNK_PREVIOUS_INUSE_FLAG | CHUNK_CURRENT_INUSE_FLAG)

/// The minimum alignment for memory addresses (16, 0x10)
#define CHUNK_ALIGNMENT TWO_SIZE_T_SIZES

/// A bit mask for aligning memory addresses (15, 0x0F, b0000 1111)
#define CHUNK_ALIGNMENT_MASK (CHUNK_ALIGNMENT - 1)

#define CHUNK_FOOTER_SIZE SIZE_T_SIZE
#define CHUNK_MIN_SIZE    (ALIGN_UP(sizeof(FreeChunk), CHUNK_ALIGNMENT))

#define CHUNK_IS_ALIGNED(CH)         (((size_t)(CH) & (CHUNK_ALIGNMENT_MASK)) == 0)
#define CHUNK_SIZE(CH)               ((CH)->size & ~CHUNK_INUSE_FLAGS)
#define CHUNK_SET_INUSE(CH)          ((CH)->size |= CHUNK_CURRENT_INUSE_FLAG)
#define CHUNK_SET_FREE(CH)           ((CH)->size &= ~CHUNK_CURRENT_INUSE_FLAG)
#define CHUNK_SET_PREVIOUS_INUSE(CH) ((CH)->size |= CHUNK_PREVIOUS_INUSE_FLAG)

// is the chunk in use?
#define CHUNK_IS_INUSE(CH) (((CH)->size & CHUNK_CURRENT_INUSE_FLAG) != 0)

// is the chunk's prior chunk in use?
#define CHUNK_IS_PREVIOUS_INUSE(CH) (((CH)->size & CHUNK_PREVIOUS_INUSE_FLAG) != 0)

// clear the previous in-use flag from the chunk
#define CHUNK_CLEAR_PREVIOUS_INUSE(CH) ((CH)->size &= ~CHUNK_PREVIOUS_INUSE_FLAG)

#define CHUNK_ALIGNED_SIZE ALIGN_UP(sizeof(Chunk), CHUNK_ALIGNMENT)

/// the payload always starts CHUNK_ALIGNED_SIZE after the address of the chunk.
#define CHUNK_TO_MEMORY(CH)    ((void *)(((char *)(CH)) + CHUNK_ALIGNED_SIZE))
#define CHUNK_FROM_MEMORY(MEM) chunk_from_memory_validate((MEM), __FILE__, __LINE__)
#define CHUNK_FROM_MEMORY_FILE_LINE(MEM, FILE, LINE) \
    chunk_from_memory_validate((MEM), (FILE), (LINE))
#define FREE_CHUNK_FROM_MEMORY(MEM, FILE, LINE) \
    ((FreeChunk *)chunk_from_memory_validate((MEM), (FILE), (LINE)))

/**
 * @brief The maximum request size. A larger request would cause the size of a chunk
 * large enough to contain this request to wrap around the values that are representable
 * by a 64-bit size_t.
 */
#define CHUNK_MAX_REQUEST ALIGN_DOWN(MAX_SIZE_T - CHUNK_ALIGNED_SIZE, CHUNK_ALIGNMENT)

/**
 * @brief The minimum payload is the space used by a FreeChunk less the space for its
 * size field, because that must be preserved between allocations and frees.
 */
#define CHUNK_MIN_PAYLOAD (CHUNK_MIN_SIZE - CHUNK_ALIGNED_SIZE)

/// the payload is the space in a chunk after the header.
#define CHUNK_REQUEST_TO_PAYLOAD(REQ) \
    ((REQ) <= CHUNK_MIN_PAYLOAD ? CHUNK_MIN_PAYLOAD : ALIGN_UP((REQ), CHUNK_ALIGNMENT))

/**
 * @brief Check if a request is so large that it would wrap around zero when padded and
 * aligned. To simplify some other code, the boundary is made low enough so that adding
 * CHUNK_MIN_SIZE will not wrap around zero.
 */
#define CHUNK_REQUEST_OUT_OF_RANGE(REQ) ((size_t)(REQ) > (size_t)CHUNK_MAX_REQUEST)

#define CHUNK_PRODUCT_OUT_OF_RANGE(COUNT, SIZE) \
    ((size_t)(COUNT) > (size_t)CHUNK_MAX_REQUEST / (size_t)(SIZE))

/// The smallest size Chunk that is large enough to satisfy the request.
#define CHUNK_SIZE_FROM_REQUEST(REQ)             \
    ((REQ) <= CHUNK_MIN_PAYLOAD ? CHUNK_MIN_SIZE \
                                : CHUNK_ALIGNED_SIZE + ALIGN_UP(REQ, CHUNK_ALIGNMENT))

// The maximum payload that this chunk can support
#define CHUNK_PAYLOAD_SIZE(CH) (CHUNK_SIZE(CH) - CHUNK_ALIGNED_SIZE)

/**
 * @brief The address of the next chunk in committed memory.
 *
 * If the given chunk is the last one in committed memory, then the returned address WILL
 * BE INVALID!
 */
#define CHUNK_NEXT(CH)      ((Chunk *)((char *)(CH) + CHUNK_SIZE(CH)))
#define FREE_CHUNK_NEXT(CH) ((FreeChunk *)((char *)(CH) + CHUNK_SIZE(CH)))

#define FREE_CHUNK_FROM_SIBLINGS(S) FL_CONTAINER_OF(S, FreeChunk, siblings)
#define FREE_CHUNK_TO_SIBLINGS(CH)  (&(CH)->siblings)

#define CHUNK_VALIDATE(CH, SIZE)                                \
    do {                                                        \
        FL_ASSERT_NOT_NULL(CH);                                 \
        size_t ch_size_ = CHUNK_SIZE(CH);                       \
                                                                \
        FL_ASSERT_ZERO_SIZE_T(ch_size_ & CHUNK_ALIGNMENT_MASK); \
        FL_ASSERT_GE_SIZE_T(ch_size_, CHUNK_MIN_SIZE);          \
        FL_ASSERT_GE_SIZE_T(ch_size_, (SIZE));                  \
    } while (0)

#define CHUNK_SENTINEL_SIZE CHUNK_ALIGNED_SIZE

/**
 * @brief Initialize the sentinel. It is the chunk that follows top. This macro sets its
 * size and sets its in-use bit to ensure it's not consolidated with top.
 *
 * This assumes the top chunk has been initialized and its size has left
 * CHUNK_SENTINEL_SIZE bytes less than all available space.
 *
 */
#define CHUNK_SET_SENTINEL(TOP)                \
    do {                                       \
        Chunk *SENTINEL = CHUNK_NEXT(TOP);     \
        SENTINEL->size  = CHUNK_SENTINEL_SIZE; \
        CHUNK_SET_INUSE(SENTINEL);             \
    } while (0)

/**
 * @brief The minimum information about a chunk of memory is its size and whether or not
 * it is in use and whether the previous chunk is in use. All chunks are 16-byte aligned
 * (CHUNK_ALIGNMENT), so the least significant 4 bits are available to use as flags for
 * in-use and previous in-use.
 *
 * @param size the size of the chunk in bytes including this size field.
 */
typedef struct Chunk {
    size_t size;
    size_t pad0; // for 16-byte alignment
} Chunk;

/**
 * @brief FreeChunk is like Chunk, but adds sibling linkages to keep like-sized chunks in
 * a list and stores a copy of its size in the footer.
 *
 * The footer takes up the last 8 bytes (sizeof(size_t)) of the chunk itself. This way,
 * the next chunk in memory will have its previous in-use bit cleared and then be able to
 * use this chunk's footer to find the address of this chunk. This capability is
 * useful for memory allocators that want to consolidate newly freed chunks with
 * previously freed chunks, reducing memory fragmentation in the process. It's up to
 * those memory allocators to ensure that all allocated chunks are of sufficient size
 * that there's room for the sibling linkages and a footer.
 *
 * @param size the size of the chunk in bytes with embedded in-use bits
 * @param siblings linkage to make a circular, doubly-linked list of like-sized free
 * chunks
 */
typedef struct FreeChunk {
    size_t size;
    DList  siblings;
    size_t footer;
} FreeChunk;

/**
 * @brief Return the size of the previous chunk in memory.
 *
 * It is an unchecked error to call this function if the previous chunk is in use.
 *
 * @param ch the chunk that follows the one whose size we want.
 * @return size_t the size, in bytes, of the previous chunk
 */
static inline size_t chunk_previous_size(Chunk *ch) {
    return *((size_t *)((char *)ch - CHUNK_FOOTER_SIZE));
}

/**
 * @brief Return the address of the previous chunk in memory.
 *
 * It is an unchecked error to call this function if the previous chunk is in use.
 *
 * @param ch the address of the current chunk
 * @return FreeChunk* the address of the previous chunk
 */
static inline FreeChunk *chunk_get_previous(Chunk *ch) {
    return (FreeChunk *)((char *)ch - chunk_previous_size(ch));
}

/**
 * @brief get the address of the previous chunk's footer.
 *
 * It is an unchecked error to read the value from this address if the previous chunk is
 * in use.
 *
 * @param ch
 * @return size_t*
 */
static inline size_t *chunk_previous_footer(Chunk *ch) {
    return (size_t *)(((char *)(ch)) - CHUNK_FOOTER_SIZE);
}

/**
 * @brief get the address of the chunk's footer
 *
 * @param ch
 * @return size_t*
 */
static inline size_t *free_chunk_get_footer(FreeChunk *ch) {
    return (size_t *)((char *)ch + CHUNK_SIZE(ch) - CHUNK_FOOTER_SIZE);
}

/// Read the size stored in the chunk's footer.
static inline size_t free_chunk_footer_size(FreeChunk *ch) {
    return *free_chunk_get_footer(ch);
}

/**
 * @brief Initialize a free chunk with its size and whether or not the previous chunk in
 * memory is in use.
 *
 * @param ch the chunk to initialize
 * @param size the chunk's size in bytes
 * @param previous_inuse true if the previous chunk is in use and false otherwise
 */
static inline void free_chunk_init(FreeChunk *ch, size_t size, bool previous_inuse) {
    ch->size = size;
    if (previous_inuse) {
        CHUNK_SET_PREVIOUS_INUSE(ch);
    }

    DLIST_INIT(&ch->siblings);
    // The actual footer is in the last 8 bytes of the chunk's memory block. Setting the
    // footer field is helpful for debugging even if it's not the actual footer.
    ch->footer = size;
    if (size > CHUNK_MIN_SIZE) { // size > 32
        size_t *footer = free_chunk_get_footer(ch);
        *footer        = size;
    }
}

/**
 * @brief free a chunk that was in use. It's safe to assume that its size is already set.
 *
 * - clear its in-use flag
 * - initialize its siblings list
 * - copy its size to its footer
 * - clear the next chunk's previous in-use flag
 *
 * @param ch
 */
static inline void free_chunk_init_simple(FreeChunk *ch) {
    CHUNK_SET_FREE(ch);
    DLIST_INIT(&ch->siblings);
    size_t size = CHUNK_SIZE(ch);
    ch->footer  = size;
    if (size > CHUNK_MIN_SIZE) {
        size_t *footer = free_chunk_get_footer(ch);
        *footer        = size;
    }
    CHUNK_CLEAR_PREVIOUS_INUSE(CHUNK_NEXT(ch));
}

/**
 * @brief free a previously in-use chunk
 *
 * @param ch
 * @return FreeChunk*
 */
static inline FreeChunk *chunk_free(Chunk *ch) {
    free_chunk_init_simple((FreeChunk *)ch);
    return (FreeChunk *)ch;
}

/// Return true if this chunk has siblings in the free list.
static inline bool free_chunk_has_siblings(FreeChunk *ch) {
    return !DLIST_IS_EMPTY(&ch->siblings);
}

/// Return the next sibling in the circular free list.
static inline FreeChunk *free_chunk_next_sibling(FreeChunk *ch) {
    return FL_CONTAINER_OF(DLIST_NEXT(&ch->siblings), FreeChunk, siblings);
}

/// Return the previous sibling in the circular free list.
static inline FreeChunk *free_chunk_previous_sibling(FreeChunk *ch) {
    return FL_CONTAINER_OF(DLIST_PREVIOUS(&ch->siblings), FreeChunk, siblings);
}

/**
 * @brief split ch into two chunks where the size of the original chunk is now size and
 * the size of the remaining chunk is size bytes less than the original chunk.
 *
 * If the remainder is not large enough to create a FreeChunk, then ch is not split, the
 * whole chunk is in-use and NULL is returned. If size > chunk_size(ch), throw an
 * exception.
 *
 * @param ch the FreeChunk to be split
 * @param size the new size of ch
 * @return FreeChunk* the address of the remainder or NULL if no remainder can be created
 * @throw fl_invalid_value if size > chunk_size(ch)
 */
static inline FreeChunk *free_chunk_split(FreeChunk *ch, size_t new_size,
                                          char const *file, int line) {
    size_t size = CHUNK_SIZE(ch);
    if (new_size > size || new_size < CHUNK_MIN_SIZE
        || (new_size % CHUNK_ALIGNMENT) != 0) {
        FL_THROW_DETAILS_FILE_LINE(fl_invalid_value, "invalid size: %zu", file, line,
                                   new_size);
    }

    flag64     in_use         = ch->size & CHUNK_INUSE_FLAGS;
    size_t     remainder_size = size - new_size;
    FreeChunk *remainder      = NULL;
    if (remainder_size >= CHUNK_MIN_SIZE) {
        // maintain the original in-use flags
        ch->size   = new_size | in_use;
        ch->footer = new_size;
        if (new_size > CHUNK_MIN_SIZE) {
            size_t *footer = free_chunk_get_footer(ch);
            *footer        = new_size;
        }

        remainder = (FreeChunk *)CHUNK_NEXT(ch);
        free_chunk_init(remainder, remainder_size, true);
    }

    return remainder;
}

/**
 * @brief merge ch with the previous chunk in memory.
 *
 * The previous chunk must not be in use.
 *
 * @param ch the chunk to be merged.
 * @return FreeChunk* the address of the previous chunk delicately merged with the
 * current one.
 */
static inline FreeChunk *free_chunk_merge_previous(FreeChunk *ch) {
    FreeChunk *prev   = chunk_get_previous((Chunk *)ch);
    size_t    *footer = free_chunk_get_footer(ch);
    size_t     total  = CHUNK_SIZE(prev) + CHUNK_SIZE(ch);
    prev->size        = total | (prev->size & CHUNK_PREVIOUS_INUSE_FLAG);
    prev->footer      = total;
    *footer           = total;
    return prev;
}

/**
 * @brief merge a chunk with the next one in memory.
 *
 * - get the address of the next chunk
 * - get the address of its footer
 * - calculate the size of the combined chunk
 * - set the merged chunk's size preserving the state of its previous in-use flag
 * - write the total size to its footer
 *
 * @param ch
 */
static inline void free_chunk_merge_next(FreeChunk *ch) {
    FreeChunk *next   = FREE_CHUNK_NEXT(ch);
    size_t    *footer = free_chunk_get_footer(next);
    size_t     total  = CHUNK_SIZE(ch) + CHUNK_SIZE(next);
    // preserve the state of the previous in-use flag
    ch->size   = total | (ch->size & CHUNK_PREVIOUS_INUSE_FLAG);
    ch->footer = total;
    *footer    = total;
}

/// Insert ch2 as a sibling after ch1 in the free list.
static inline void free_chunk_insert(FreeChunk *ch1, FreeChunk *ch2) {
    DLIST_INSERT_NEXT(&ch1->siblings, &ch2->siblings);
}

/// Remove ch from its sibling list.
static inline void free_chunk_remove(FreeChunk *ch) {
    DLIST_REMOVE(&ch->siblings);
}

/// Mark a free chunk as in-use and set the next chunk's previous-inuse flag.
static inline void free_chunk_set_inuse(FreeChunk *ch) {
    CHUNK_SET_INUSE(ch);
    CHUNK_SET_PREVIOUS_INUSE(CHUNK_NEXT(ch));
}

/**
 * @brief verify memory address is not NULL, and is aligned before converting to a chunk
 *
 * @param mem
 * @param file
 * @param line
 * @return Chunk*
 */
static inline Chunk *chunk_from_memory_validate(void *mem, char const *file, int line) {
    if (mem == NULL) {
        FL_THROW_DETAILS_FILE_LINE(fl_invalid_address, "memory pointer is NULL", file,
                                   line);
    }

    // Check alignment
    if (((uintptr_t)mem & CHUNK_ALIGNMENT_MASK) != 0) {
        FL_THROW_DETAILS_FILE_LINE(fl_invalid_address,
                                   "memory pointer 0x%p is not aligned to %zu bytes",
                                   file, line, mem, CHUNK_ALIGNMENT);
    }

    Chunk *ch = (Chunk *)((char *)mem - CHUNK_ALIGNED_SIZE);

    // Validate chunk header
    if (!CHUNK_IS_ALIGNED(ch)) {
        FL_THROW_DETAILS_FILE_LINE(fl_invalid_address,
                                   "chunk header 0x%p is not aligned", file, line, ch);
    }

    return ch;
}

#endif // CHUNK_H_
