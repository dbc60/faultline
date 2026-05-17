#ifndef ARENA_INTERNAL_H_
#define ARENA_INTERNAL_H_

/**
 * @file arena_internal.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2007-09-10
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "chunk.h"
#include "digital_search_tree.h"
#include "region.h"      // Region
#include "region_node.h" // RegionNode

#include <faultline/dlist.h>                // DList
#include <faultline/fl_abbreviated_types.h> // FL_COMPARE
#include <faultline/fl_exception_types.h>   // FLExceptionReason
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT* and fl_unexpected_failure declaration
#include <faultline/fl_try.h>                      // FL_THROW* macros
#include <faultline/size.h>                        // SIZE_ macros

#include <stddef.h> // size_t
#include <stdio.h>  // snprintf

// LOG2(ARENA_MIN_LARGE_CHUNK)
#define ARENA_LOG2_MIN_LARGE_CHUNK 10
#define ARENA_DETAILS_SIZE         256

/// runtime checks
#define ARENA_RTCHECK(EXP, FILE, LINE)                         \
    do {                                                       \
        if (!(EXP)) {                                          \
            FL_THROW_FILE_LINE(fl_internal_error, FILE, LINE); \
        }                                                      \
    } while (0)

#define ARENA_RTCHECK_VA(EXP, FILE, LINE, fmt, ...)              \
    do {                                                         \
        if (!(EXP)) {                                            \
            static char details[ARENA_DETAILS_SIZE];             \
            snprintf(details, sizeof details, fmt, __VA_ARGS__); \
            fl_throw(fl_internal_error, details, FILE, LINE);    \
        }                                                        \
    } while (0)

/// the maximum size of a chunk that can fit into a small bin
#define ARENA_MAX_SMALL_CHUNK \
    (CHUNK_MIN_SIZE + (ARENA_SMALL_BIN_COUNT - 1) * CHUNK_ALIGNMENT)

/**
 * @brief The first small bin covers chunks of CHUNK_MIN_SIZE bytes. The remaining small
 * bins each cover a request range of 16 bytes each. If there are 61 bins, then the
 * maximum request that will fit into a small bin is 992
 */
#define ARENA_MAX_SMALL_REQUEST (ARENA_MAX_SMALL_CHUNK - CHUNK_ALIGNED_SIZE)

#define ARENA_DEFAULT_TRIM_THRESHOLD ((size_t)2U * (size_t)MEBI(1))
#define ARENA_ALIGNED_SIZE           ALIGN_UP(sizeof(Arena), TWO_SIZE_T_SIZES)

/* ---------------------------- Indexing Bins ---------------------------- */

/**
 * @brief A chunk is small if it's no larger than ARENA_MAX_SMALL_CHUNK, because the
 * first small bin holds chunks whose size are CHUNK_MIN_SIZE and the next
 * (ARENA_SMALL_BIN_COUNT-1) hold chunks that are each 16 bytes larger than the previous
 * one.
 */
#define ARENA_IS_CHUNK_SMALL(ch) (CHUNK_SIZE(ch) <= ARENA_MAX_SMALL_CHUNK)

/**
 * @brief
 *
 */
#define ARENA_IS_CHUNK_LARGE(ch) (CHUNK_SIZE(ch) >= ARENA_MIN_LARGE_CHUNK)

#define ARENA_IS_SIZE_SMALL(SZ) ((SZ) <= ARENA_MAX_SMALL_CHUNK)

/// convert the size of a small chunk to an index.
#define ARENA_SMALL_INDEX(s) (((u32)((s) - CHUNK_MIN_SIZE)) / CHUNK_ALIGNMENT)

/// convert a small-bin index to a chunk size.
#define ARENA_SMALL_INDEX_TO_SIZE(I) ((I) * CHUNK_ALIGNMENT + CHUNK_MIN_SIZE)

#define MS_END_OF_MEMORY(ms) \
    (((Region *)((char *)(ms) - REGION_ALIGNED_SIZE))->end_committed)

#define MS_IS_NEXT_CHUNK_VALID(ch) \
    ((ptrdiff_t)CHUNK_NEXT(ch) < (ptrdiff_t)MS_END_OF_MEMORY(ch->hdr.arena))

/// The power of 2 to create a mask for small bins that are exact-fit candidates.
#define ARENA_SMALL_BIN_POW2 (1 << CHUNK_MIN_SIZE / CHUNK_ALIGNMENT)

/**
 * @brief A mask to select small bins that are exact-fit candidates for an allocation
 * request.
 */
#define ARENA_INDIVISIBLE_SMALL_BIN_MASK \
    (~ARENA_SMALL_BIN_POW2 & (ARENA_SMALL_BIN_POW2 - 1))

/**
 * @brief mask out bits in SBM, where SBM represents the bits in the small-bin map
 * adjusted for the bin index, and return a bit mask for selecting the exact-fit bins.
 */
#define ARENA_SBM(SBM) ((SBM) & ARENA_INDIVISIBLE_SMALL_BIN_MASK)

/// Adjust the bits in the small-bin map based on the index of the given chunk size S.
#define ARENA_SMALL_BITS(ARENA, S) ((ARENA)->small_map >> ARENA_SMALL_INDEX(S))

/**
 * @brief return a non-zero value if the arena has at least one exact-fit bin for the
 * given chunk size S.
 */
#define ARENA_HAS_REMAINDERLESS_SMALL_BIN(ARENA, S) \
    (ARENA_SMALL_BITS(ARENA, S) & ARENA_INDIVISIBLE_SMALL_BIN_MASK)

/**
 * @brief Given a bin map BM, return the
 *
 * This macro is valid for header sizes of 8, 16, 24, and 32 bytes. If you're using a
 * larger header (fascinating!), please extend or replace this macro.
 */
#define ARENA_SMALL_BIN_INCREMENT(BM)                            \
    (~FL_COMPARE(ARENA_SBM(BM), 0)                               \
     & ((~ARENA_SBM(BM) & 0x1) + ((~ARENA_SBM(BM) & 0x3) == 0x3) \
        + ((~ARENA_SBM(BM) & 0x7) == 0x7)))

/**
 * @brief Given the size of a small chunk, return the index of the first bin with a chunk
 * large enough to satisfy the request, but too small to split. The index is valid only
 * if ARENA_HAS_REMAINDERLESS_SMALL_BIN(ARENA, S) is true.
 *
 * This macro is valid for header sizes of 8, 16, 24, and 32 bytes. If you're using a
 * larger header (fascinating!), please extend or replace this macro.
 */
#define ARENA_REMAINDERLESS_SMALL_INDEX(ARENA, S) \
    (ARENA_SMALL_INDEX(S) + ARENA_SMALL_BIN_INCREMENT(ARENA_SMALL_BITS(ARENA, S)))

/// return the address of the small bin at index I.
#define ARENA_SMALL_BIN_AT(AR, I) (&(AR)->small_bins[(I)])

/// return the address of the large bin at index I.
#define ARENA_LARGE_BIN_AT(AR, I) (&(AR)->large_bins[I])

/* ------------------------ Operations on bin maps ----------------------- */

/* bit corresponding to given index */
#define IDX_TO_BIT32(I) ((u32)1 << (I))
#define IDX_TO_BIT64(I) ((u64)1 << (I))

/* Mark/Clear bits with given index */
#define ARENA_MARK_SMALL_MAP(AR, I)      ((AR)->small_map |= IDX_TO_BIT64(I))
#define ARENA_CLEAR_SMALL_MAP(AR, I)     ((AR)->small_map &= ~IDX_TO_BIT64(I))
#define ARENA_SMALL_MAP_IS_MARKED(AR, I) ((AR)->small_map & IDX_TO_BIT64(I))

#define ARENA_MARK_LARGE_MAP(AR, I)      ((AR)->large_map |= IDX_TO_BIT64(I))
#define ARENA_CLEAR_LARGE_MAP(AR, I)     ((AR)->large_map &= ~IDX_TO_BIT64(I))
#define ARENA_LARGE_MAP_IS_MARKED(AR, I) (((AR)->large_map & IDX_TO_BIT64(I)) != 0)

/**
 * @brief A rough determination of whether an address is contained within one of this
 * arena's regions.
 *
 * @param arena the arena in which addr should exist.
 * @param addr the address to test.
 * @return true if addr is contained in one of this arena's regions and false otherwise.
 */
#define ARENA_ADDRESS_VALID(AR, ADDR, FILE, LINE)               \
    do {                                                        \
        if (!arena_address_ok(AR, ADDR))                        \
            FL_THROW_FILE_LINE(fl_invalid_address, FILE, LINE); \
    } while (0)

/**
 * @brief The default rate for releasing unused regions.
 */
#define MAX_RELEASE_CHECK_RATE 4095

/**
 * @brief Replace the fast node, binning the old one.
 *
 * Used only when fast_size known to be small.
 */
#define ARENA_REPLACE_FAST_NODE(AR, CH, FILE, LINE)                       \
    do {                                                                  \
        size_t     fast_size_ = (AR)->fast_size;                          \
        FreeChunk *ch_        = (CH);                                     \
        FL_ASSERT_FILE_LINE(ARENA_IS_SIZE_SMALL(fast_size_), FILE, LINE); \
        if (fast_size_ != 0) {                                            \
            insert_small_chunk(AR, (AR)->fast, FILE, LINE);               \
        }                                                                 \
        (AR)->fast      = ch_;                                            \
        (AR)->fast_size = ch_ != NULL ? CHUNK_SIZE(ch_) : 0;              \
    } while (0)

/******************************************************************************/
/******************************************************************************/

/*
 *  Arena Representation (assuming 64-bit, little endian):
 *
 *  byte:   |       7       6       5       4       3       2       1       0|
 *          |7654321076543210765432107654321076543210765432107654321076543210|
 *  state-> |+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+-|
 *    0x0000|                                                     initialized| <=
 * generally 1 byte 0x0008| first| <= compiler forces 8-byte alignment 0x0010| current|
 *    0x0018|                      large_map|                       small_map|
 *    0x0020|                                                  fast_size|
 *    0x0028|                                                             top|
 *    0x0030|                                                      small_bins|
 *    0x0230|                                                      large_bins|
 *    0x0430|                                                      least_addr|
 *    0x0438|                                                       fast|
 *    0x0440|                                                      trim_check|
 *    0x0448|                                                  release_checks|
 *    0x0450|                                                       footprint|
 *    0x0458|                                                   max_footprint|
 *    0x0460|                                                 footprint_limit|
 *    0x0468|                         blocks|                          mflags|
 *    0x0470|                               |                           count|
 * end      |+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+|+-+-+-+-|
 * total size: 0x0478 bytes, or 1,144 bytes.
 */
/// The minimum size of large chunks
#define ARENA_MIN_LARGE_CHUNK         1024
#define ARENA_LARGE_BIN_COUNT_DEFAULT 48
#ifndef ARENA_LARGE_BIN_COUNT
#define ARENA_LARGE_BIN_COUNT ARENA_LARGE_BIN_COUNT_DEFAULT
#endif
#define ARENA_CHUNK_TO_DST(CH)       ((DigitalSearchTree *)(CH))
#define ARENA_DST_TO_FREE_CHUNK(DST) ((FreeChunk *)(DST))

/**
 * @brief The number of small bins in the arena.
 *
 * Except for the first one, each small bin is 16 bytes wide for double-word alignment on
 * a 64-bit OS. The size of the first bin is the value of CHUNK_MIN_SIZE, because it
 * makes no sense to have bins for chunks smaller than the smallest possible chunk.
 */
#define ARENA_SMALL_BIN_COUNT \
    ((ARENA_MIN_LARGE_CHUNK - CHUNK_MIN_SIZE) / CHUNK_ALIGNMENT)

/**
 * @brief ARENA_LEFT_SHIFT(IDX) takes an index (IDX) of a bin and returns the value to
 * left-shift the size of the DigitalSearchTree. The shifted value will be used to make
 * left-right decisions while searching the tree for a candidate free chunk.
 *
 * @param IDX the index of the bin.
 */
#define ARENA_LEFT_SHIFT(IDX)               \
    (((IDX) == (ARENA_LARGE_BIN_COUNT) - 1) \
         ? 0                                \
         : (SIZE_T_BITSIZE - SIZE_T_ONE)    \
               - (((IDX) >> 1) + (ARENA_LOG2_MIN_LARGE_CHUNK) - 2))

/**
 * @brief A structure for holding the state of a memory-allocation arena.
 *
 * Note that each entry in large_bins is a pointer to the root of a digital search tree.
 * This may also be known as a trie, but some people make a small distinction between
 * between a trie and a digital search tree. A strict definition of a trie means that
 */
struct Arena {
    bool       initialized; ///< for integrity checks
    flag64     small_map;   ///< bitmap to quickly find non-empty bins.
    flag64     large_map;   ///< bitmap to quickly find non-empty bins.
    FreeChunk *fast;        ///< The preferred chunk for servicing small requests that
                            ///< don't have exact fits. It is normally the chunk split
                            ///< off most recently to service another small request.
                            ///< Its size is cached in fast_size.
    size_t fast_size;       ///< used to determine if an allocation request should be
                            ///< satisfied from the fast node.
    Chunk     *base;        ///< the initial address of top
    FreeChunk *top;         ///< the address of the remaining unallocated memory.
    DList
        region_list; ///< a list of additional regions represented as RegionNode objects
    DList small_bins[ARENA_SMALL_BIN_COUNT]; ///< Each small bin is a circular,
                                             ///< doubly-linked list.
    DigitalSearchTree
        *large_bins[ARENA_LARGE_BIN_COUNT]; ///< Each bin represents a size class and the
                                            ///< set of chunks within are maintained in a
                                            ///< digital search tree.
    size_t footprint;       ///< the number of bytes reserved from system memory.
    size_t max_footprint;   ///< a statistic tracking the largest number of bytes used.
    size_t footprint_limit; ///< A configurable maximum number of bytes that this
                            ///< allocator may use. Zero means there is no limit.
    size_t trim_check;      ///< TBD.
    size_t release_checks;  ///< a counter that is initialized to MAX_RELEASE_CHECK_RATE
                            ///< and decremented once per call to free(). When it is
                            ///< zero, free any Regions that don't contain used chunks
                            ///< and reset the counter to MAX_RELEASE_CHECK_RATE or the
                            ///< current number of regions, whichever is greater.
    size_t allocations;
};
typedef struct Arena Arena;

extern FLExceptionReason arena_out_of_memory;
extern FLExceptionReason fl_internal_error;

/**
 * @brief Check if an address is within the bounds of any region in the arena.
 *
 * This function checks both the primary region (base/top) and all additional
 * regions in the region_list to support non-contiguous memory expansion.
 *
 * The check uses 'top' (the wilderness chunk pointer) rather than end_committed
 * to match the original ARENA_ADDRESS_OK macro behavior - we only validate addresses
 * in the allocatable/allocated space, not in internal structures like the wilderness
 * chunk or sentinel.
 *
 * @param arena the arena to check
 * @param addr the address to validate
 * @return true if addr is within any of the arena's regions, false otherwise
 */
static inline bool arena_address_ok(Arena const *arena, void const *addr) {
    ptrdiff_t address = (ptrdiff_t)addr;

    // Check primary region - from base to wilderness chunk (top)
    if (address >= (ptrdiff_t)arena->base && address < (ptrdiff_t)arena->top) {
        return true;
    }

    // Check additional regions in region_list
    if (!DLIST_IS_EMPTY(&arena->region_list)) {
        DList *entry = DLIST_NEXT(&arena->region_list);

        while (entry != &arena->region_list) {
            RegionNode *node = FL_CONTAINER_OF(entry, RegionNode, link);

            // Check from node's base to its wilderness chunk (top)
            if (address >= (ptrdiff_t)node->base && address < (ptrdiff_t)node->top) {
                return true;
            }

            entry = DLIST_NEXT(entry);
        }
    }

    return false;
}

#endif // ARENA_INTERNAL_H_
