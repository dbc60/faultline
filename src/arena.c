/**
 * @file arena.c
 * @author Douglas Cuthbertson
 * @brief An implementation of an arena memory allocator.
 * @version 0.1
 * @date 2007-09-10
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/arena.h>
#include <faultline/dlist.h>           // for DLIST_NEXT, DList, DLIST_IS...
#include <faultline/fl_abbreviated_types.h>        // for u32, flag64, u64
#include <faultline/fl_exception_service_assert.h> // for assert, FL_ASSERT_DETAILS_F...
#include <faultline/fl_exception_types.h>          // for FLExceptionReason
#include <faultline/fl_log.h>                      // for LOG_DEBUG, LOG_ERROR
#include <faultline/fl_macros.h>                   // for FL_CONTAINER_OF
#include <faultline/fl_try.h>                      // for FL_THROW_DETAILS_FILE_LINE
#include <faultline/size.h>                        // for TWO_SIZE_T_SIZES, MAX
#include <stdbool.h>                     // for bool, true
#include <stddef.h>                      // for size_t, NULL
#include <stdint.h>                      // for uintptr_t
#include <string.h>                      // for memcpy, memset
#include "arena_dbg.h"                   // for ARENA_CHECK_ALLOCATED_CHUNK
#include "arena_internal.h"              // for Arena, ARENA_RTCHECK, arena...
#include "bits.h"                        // for BIT_LSB, ALIGN_UP, BIT_MASK...
#include "chunk.h"                       // for CHUNK_SIZE, FreeChunk, Chunk
#include "digital_search_tree.h"         // for DigitalSearchTree, dst_init...
#include "index.h"                       // for INDEX_BY_VALUE64, COMPUTE_L...
#include "region.h"                      // for MEM_TO_REGION, REGION_BYTES...
#include "region_node.h"                 // for RegionNode, new_region_node
#include "win32_platform.h"              // for get_memory_info

FLExceptionReason arena_out_of_memory = "arena out of memory";

/**
 * @brief malloc_params holds global properties, including those that can be dynamically
 * set using mallopt. There is a single instance, mparams, initialized in init_mparams.
 */
struct malloc_params {
    bool   initialized;
    size_t page_size;
    size_t granularity;
    size_t trim_threshold;
};

static struct malloc_params mparams;

#define M_TRIM_THRESHOLD (-1)
#define M_GRANULARITY    (-2)

/* TODO: use to support mallopt functionality */
#ifdef NOT_DEF
static int change_mparam(int param_number, int value) {
    if (value < -1) {
        return 0; // Invalid parameter
    }

    size_t val;
    ensure_initialization();
    val = (value == -1) ? MAX_SIZE_T : (size_t)value;

    switch (param_number) {
    case M_TRIM_THRESHOLD:
        mparams.trim_threshold = val;
        return 1;
    case M_GRANULARITY:
        if (val >= mparams.page_size && ((val & (val - 1)) == 0)) {
            mparams.granularity = val;
            return 1;
        } else
            return 0;
    default:
        return 0;
    }
}
#endif // NOT_DEF

// ============================================================================
// System Information
// ============================================================================

void init_mparams(void) {
    if (!mparams.initialized) {
        u32 page_size;
        u32 granularity;

        get_memory_info(&granularity, &page_size);
        mparams.granularity    = granularity;
        mparams.page_size      = page_size;
        mparams.trim_threshold = 0;
        mparams.initialized    = true;
    }
}

/**
 * @brief Get the operating system's page size.
 *
 * @return The page size in bytes (typically 4096 bytes on most systems).
 */
size_t arena_page_size(void) {
    init_mparams();
    return mparams.page_size;
}

/**
 * @brief Get the operating system's memory allocation granularity.
 *
 * Memory regions are allocated in multiples of this size.
 *
 * @return The allocation granularity in bytes (typically 65536 bytes on Windows).
 */
size_t arena_granularity(void) {
    init_mparams();
    return mparams.granularity;
}

Arena *new_arena(size_t commit, u32 reserve) {
    FL_ASSERT_DETAILS(commit <= SIZE_MAX - ARENA_ALIGNED_SIZE - CHUNK_SENTINEL_SIZE
                                    - TWO_SIZE_T_SIZES,
                      "request size too large");

    // we need space for the arena + commit + sentinel
    size_t size
        = ARENA_ALIGNED_SIZE + ALIGN_UP(commit, TWO_SIZE_T_SIZES) + CHUNK_SENTINEL_SIZE;
    Region *region = new_region(size, reserve);

    Arena *arena       = REGION_TO_MEM(region);
    arena->initialized = true;
    arena->base        = (Chunk *)(((char *)arena) + ARENA_ALIGNED_SIZE);
    arena->top         = (FreeChunk *)arena->base;
    DLIST_INIT(&arena->region_list);

    // available allocation space is the number of bytes committed less what are already
    // consumed by the region, arena, and sentinel.
    size_t const committed = REGION_BYTES_COMMITTED(region);
    size_t       top_size
        = committed - REGION_ALIGNED_SIZE - ARENA_ALIGNED_SIZE - CHUNK_SENTINEL_SIZE;
    free_chunk_init(arena->top, top_size, true);
    CHUNK_SET_SENTINEL(arena->top);

    for (size_t i = 0; i < ARENA_SMALL_BIN_COUNT; i++) {
        DLIST_INIT(&arena->small_bins[i]);
    }

    arena->footprint     = committed;
    arena->max_footprint = committed;

    /** note that trim_check could/should be configurable through a global
     * struct malloc_params that's initialized at startup. */
    // arena->trim_check     = ARENA_DEFAULT_TRIM_THRESHOLD;
    arena->release_checks = MAX_RELEASE_CHECK_RATE;

    // return the address of the first byte after the arena
    return arena;
}

void release_arena(Arena **arena) {
    DList *head = &(*arena)->region_list;
    DList *next = DLIST_NEXT(head);
    while (next != head) {
        RegionNode *rn = FL_CONTAINER_OF(next, RegionNode, link);
        DLIST_REMOVE_SIMPLE(next);
        release_region_node(rn);
        next = DLIST_NEXT(head);
    }

    release_region(MEM_TO_REGION(*arena));
    *arena = NULL;
}

/**
 * @brief insert a small initialized Chunk somewhere among the arena's small bins.
 *
 * @param arena the arena in which to insert the Chunk.
 * @param chunk the Chunk to be inserted.
 */
static void insert_small_chunk(Arena *arena, FreeChunk *ch, char const *file, int line) {
    u32        index = ARENA_SMALL_INDEX(CHUNK_SIZE(ch));
    DList     *bin   = ARENA_SMALL_BIN_AT(arena, index);
    FreeChunk *next  = FREE_CHUNK_FROM_SIBLINGS(bin);
    FL_ASSERT_GE_SIZE_T(CHUNK_SIZE(ch), CHUNK_MIN_SIZE);
    if (!ARENA_SMALL_MAP_IS_MARKED(arena, index)) {
        ARENA_MARK_SMALL_MAP(arena, index);
    } else {
        ARENA_RTCHECK(arena_address_ok(arena, DLIST_NEXT(bin)), file, line);
        next = free_chunk_next_sibling(next);
    }
    free_chunk_insert(next, ch);
}

/**
 * @brief insert a node in one of the arena's large bins.
 *
 * This function assumes the node has been initialized so its size, index, sibling links,
 * and child links are all correctly set.
 *
 * @param arena the arena into which the node will be inserted.
 * @param node the initialized tree node to be inserted into a large bin.
 */
static void insert_large_chunk(Arena *arena, DigitalSearchTree *node, char const *file,
                               int line) {
    u32 index;
    INDEX_BY_VALUE64(CHUNK_SIZE(node), ARENA_LARGE_BIN_COUNT, ARENA_LOG2_MIN_LARGE_CHUNK,
                     index);
    DigitalSearchTree **bin = ARENA_LARGE_BIN_AT(arena, index);
    FL_ASSERT_TRUE(DST_IS_ROOT(node));
    FL_ASSERT_TRUE(DST_IS_LEAF(node));
    FL_ASSERT_TRUE(!DST_HAS_SIBLINGS(node));

    if (*bin == NULL) {
        ARENA_RTCHECK(!ARENA_LARGE_MAP_IS_MARKED(arena, index), file, line);
        // insert the node as the root of the bin.
        *bin = node;
        ARENA_MARK_LARGE_MAP(arena, index);
    } else {
        ARENA_RTCHECK(ARENA_LARGE_MAP_IS_MARKED(arena, index), file, line);
        dst_insert(*bin, node, ARENA_LEFT_SHIFT(index));
    }
    ARENA_RTCHECK(ARENA_LARGE_MAP_IS_MARKED(arena, index), file, line);
}

/**
 * @brief allocate a remainderless, exact-fit chunk from a small bin
 *
 * @param arena
 * @param size
 * @return
 */
static Chunk *allocate_exact_fit(Arena *arena, size_t size, char const *file, int line) {
    u32        index = ARENA_REMAINDERLESS_SMALL_INDEX(arena, size);
    DList     *bin   = ARENA_SMALL_BIN_AT(arena, index);
    FreeChunk *chunk = FREE_CHUNK_FROM_SIBLINGS(DLIST_NEXT(bin));

    FL_ASSERT_LT_SIZE_T(CHUNK_SIZE(chunk), (size_t)ARENA_MIN_LARGE_CHUNK);
    FL_ASSERT_EQ_SIZE_T(CHUNK_SIZE(chunk), ARENA_SMALL_INDEX_TO_SIZE(index));
    if (index < ARENA_SMALL_BIN_COUNT - 1) {
        FL_ASSERT_LT_SIZE_T(CHUNK_SIZE(chunk), ARENA_SMALL_INDEX_TO_SIZE(index + 1));
    }

    ARENA_RTCHECK(arena_address_ok(arena, chunk), file, line);
    free_chunk_remove(chunk);
    if (DLIST_IS_EMPTY(bin)) {
        ARENA_CLEAR_SMALL_MAP(arena, index);
    }

    free_chunk_set_inuse(chunk);
    return (Chunk *)chunk;
}

/**
 * @brief allocate a chunk from the fast node.
 *
 * Put any remainder back on the fast node.
 *
 * @param arena
 * @param size
 * @return a chunk of at least size bytes.
 */
static Chunk *chunk_from_fast(Arena *arena, size_t size, char const *file, int line) {
    Chunk     *ch        = (Chunk *)arena->fast;
    FreeChunk *remainder = free_chunk_split(arena->fast, size, file, line);
    if (remainder != NULL) {
        size_t remainder_size = CHUNK_SIZE(remainder);
        if (remainder_size >= ARENA_MIN_LARGE_CHUNK) {
            dst_init_minimal(ARENA_CHUNK_TO_DST(remainder));
        }
        CHUNK_SET_PREVIOUS_INUSE(remainder);
        arena->fast      = remainder;
        arena->fast_size = remainder_size;
    } else {
        arena->fast      = NULL;
        arena->fast_size = 0;
    }

    free_chunk_set_inuse((FreeChunk *)ch);
    return ch;
}

// 3. Use the chunk from the next non-empty smallbin, split it, and place the
// remainder on the fast node.
static Chunk *allocate_from_next_nonempty_small_bin(Arena *arena, size_t size,
                                                    char const *file, int line) {
    flag64 small_bits = ARENA_SMALL_BITS(arena, size);
    u64    index      = ARENA_SMALL_INDEX(size);
    flag64 left_bits  = (small_bits << index) & BIT_MASK_LEFT(IDX_TO_BIT64(index));
    flag64 least_bit  = BIT_LSB(left_bits);
    COMPUTE_LSB2IDX64(least_bit, index);
    DList     *bin = ARENA_SMALL_BIN_AT(arena, index);
    FreeChunk *ch  = FREE_CHUNK_FROM_SIBLINGS(DLIST_NEXT(bin));

    FL_ASSERT_EQ_SIZE_T(CHUNK_SIZE(ch), ARENA_SMALL_INDEX_TO_SIZE(index));
    ARENA_RTCHECK(arena_address_ok(arena, ch), file, line);

    free_chunk_remove(ch);
    if (DLIST_IS_EMPTY(bin)) {
        ARENA_CLEAR_SMALL_MAP(arena, index);
    }

    // split the chunk, set it to in-use, and put the remainder on the fast node.
    FreeChunk *remainder = free_chunk_split(ch, size, file, line);
    free_chunk_set_inuse(ch);
    if (remainder != NULL) {
        if (CHUNK_SIZE(remainder) >= ARENA_MIN_LARGE_CHUNK) {
            dst_init_minimal(ARENA_CHUNK_TO_DST(remainder));
        }
    }
    ARENA_REPLACE_FAST_NODE(arena, remainder, file, line);
    return (Chunk *)ch;
}

static void unlink_small_chunk(Arena *arena, FreeChunk *node, size_t size,
                               char const *file, int line) {
    FreeChunk *next  = free_chunk_next_sibling(node);
    FreeChunk *prev  = free_chunk_previous_sibling(node);
    u32        index = ARENA_SMALL_INDEX(size);

    FL_ASSERT_NEQ_PTR(node, prev);
    FL_ASSERT_NEQ_PTR(node, next);
    FL_ASSERT_EQ_SIZE_T(CHUNK_SIZE(node), ARENA_SMALL_INDEX_TO_SIZE(index));

    free_chunk_remove(node);
    if (prev == next) {
        ARENA_CLEAR_SMALL_MAP(arena, index);
    }

    ARENA_RTCHECK(FREE_CHUNK_TO_SIBLINGS(next) == ARENA_SMALL_BIN_AT(arena, index)
                      || (arena_address_ok(arena, next)
                          && free_chunk_previous_sibling(next) == (prev)),
                  file, line);
    ARENA_RTCHECK(FREE_CHUNK_TO_SIBLINGS(prev) == ARENA_SMALL_BIN_AT(arena, index)
                      || (arena_address_ok(arena, prev)
                          && free_chunk_next_sibling(prev) == (next)),
                  file, line);
}

/**
 * @brief Allocate a small request from the best fitting chunk in a large bin.
 *
 * Do NOT call this function if all of the large bins are empty.
 *
 * Because there's at least one node in a large bin, we're guaranteed to find one that
 * can be used to satisfy a small request.
 *
 * 1. Find the smallest large-bin to satisfy the request.
 * 2. Find the best fit chunk in that bin from the leftmost subtree.
 * 3. Remove the chunk from the bin.
 *
 * @param arena The Arena from which the chunk will be allocated.
 * @param request The number of bytes requested.
 * @return The address of a small chunk.
 */
static Chunk *allocate_small_request_from_large_bin(Arena *arena, size_t request,
                                                    char const *file, int line) {
    DigitalSearchTree *root, *victim;
    u32                index;
    size_t             size      = CHUNK_SIZE_FROM_REQUEST(request);
    u64                least_bit = BIT_LSB(arena->large_map);
    FreeChunk         *ch        = NULL;

    // get the index of the smallest large-bin that can satisfy the request
    COMPUTE_LSB2IDX64(least_bit, index);
    if (arena->large_bins[index] != NULL) {
        root              = arena->large_bins[index];
        flag64 left_shift = ARENA_LEFT_SHIFT(index);

        // look for the best-fit node (smallest remainder) in the bin
        victim = dst_find_smallest_large_node(root, size, left_shift);
        if (victim != NULL) {
            ch = ARENA_DST_TO_FREE_CHUNK(victim);
            // remove the large chunk from its bin
            DigitalSearchTree *replacement = dst_remove_leftmost(victim);
            if (victim == root) {
                // we removed the root of the bin, so replacement is the new root
                arena->large_bins[index] = replacement;
                if (replacement == NULL) {
                    ARENA_CLEAR_LARGE_MAP(arena, index);
                }
            }
            if (CHUNK_SIZE(ch) - size < CHUNK_MIN_SIZE) {
                // The chunk is too small to split, so just mark it as in-use
                free_chunk_set_inuse(ch);
            } else {
                // Split the chunk and put the remainder on the fast node.
                FreeChunk *remainder = free_chunk_split(ch, size, file, line);
                free_chunk_set_inuse(ch);
                if (remainder != NULL) {
                    if ((FreeChunk *)CHUNK_NEXT(remainder) == arena->top) {
                        // merge with top
                        arena->top = remainder;
                        free_chunk_merge_next(remainder);
                    } else {
                        if (ARENA_IS_CHUNK_LARGE(remainder)) {
                            dst_init_minimal(ARENA_CHUNK_TO_DST(remainder));
                        }

                        //  replace fast node
                        if (arena->fast_size != 0) {
                            if (ARENA_IS_SIZE_SMALL(arena->fast_size)) {
                                insert_small_chunk(arena, arena->fast, file, line);
                            } else {
                                DigitalSearchTree *tree
                                    = ARENA_CHUNK_TO_DST(arena->fast);
                                dst_init_minimal(tree);
                                insert_large_chunk(arena, tree, file, line);
                            }
                        }

                        arena->fast      = remainder;
                        arena->fast_size = CHUNK_SIZE(remainder);
                    }
                }
            }
        }
    }

    return (Chunk *)ch;
}

// 5. allocate from top.
static Chunk *allocate_from_top(Arena *arena, size_t size, char const *file, int line) {
    if (CHUNK_SIZE(arena->top) < size) {
        size_t top_size = CHUNK_SIZE(arena->top);
        LOG_ERROR("Arena", "Expected top size %zu to be >= %zu", top_size, size);
        FL_THROW_DETAILS_FILE_LINE(fl_internal_error,
                                   "Expected top size %zu to be >= %zu", file, line,
                                   top_size, size);
    }

    Chunk     *ch        = (Chunk *)arena->top;
    FreeChunk *remainder = free_chunk_split(arena->top, size, file, line);
    if (remainder == NULL) {
        arena->top = (FreeChunk *)CHUNK_NEXT(arena->top); // the sentinel
    } else {
        arena->top = remainder;
    }
    free_chunk_set_inuse((FreeChunk *)ch);

    return ch;
}

/**
 * @brief 6. allocate from the operating system. Try to extend the current Region.
 *
 * - try to commit enough memory from what's already reserved.
 * - if that's not enough, try to reserve more memory and commit from that.
 *
 * @param arena the arena from which to allocate a chunk.
 * @param request the number of bytes requested
 * @return return the address of memory allocated from arena.
 * @throw arena_out_of_memory
 */
static void *extend(Arena *arena, size_t request, char const *file, int line) {
    size_t  size      = CHUNK_SIZE_FROM_REQUEST(request);
    void   *mem       = NULL;
    Region *rgn       = MEM_TO_REGION(arena);
    size_t  available = region_available_bytes(rgn); // uncommitted, reserved/reservable
    size_t  committed;
    FreeChunk  *top;
    FreeChunk **top_addr;
    size_t      top_size    = CHUNK_SIZE(arena->top);
    size_t      extend_size = size;

    // If top_size > CHUNK_SENTINEL_SIZE, then top hasn't been exhausted, so subtract
    // top_size from size before calling region_extend so we attempt to extend the
    // region only as much as needed.
    if (top_size > CHUNK_SENTINEL_SIZE) {
        extend_size -= top_size;
    }

    LOG_DEBUG("Arena", "available: %zu, size: %zu", available, extend_size);
    if (available >= extend_size) {
        // space is available to extend the region in situ
        committed = region_extend(rgn, extend_size);
        top_addr  = &arena->top;
        top       = arena->top;
        /*
         * Think: we're here, because the remaining committed space in the arena is less
         * than the request, and we have successfully extended the arena; arena->top
         * could point to the sentinel, but it also could point to an amount of available
         * memory that was too small to satisfy the request. In the former case, extend
         * top by committed bytes. In the latter case, extend top by committed less the
         * size of the sentinel, CHUNK_SENTINEL_SIZE.
         */
        // ensure we don't add the sentinel size to the new top size
        if (CHUNK_SIZE(top) > CHUNK_SENTINEL_SIZE) {
            top_size += committed;
        } else {
            top_size = committed;
        }
        free_chunk_init(top, top_size, CHUNK_IS_PREVIOUS_INUSE(top));
        CHUNK_SET_SENTINEL(arena->top);
    } else {
        /*
         * We're here because the arena doesn't have enough space to satisfy the request
         * and the current region can't be extended enough to satisfy the request either.
         * We need to create a new RegionNode, link it to the arena's list of regions.
         */

        // Allocate a new region to satisfy the request and add it to the list
        RegionNode *node = new_region_node(size, 0);
        DLIST_INSERT_NEXT(&arena->region_list, &node->link);
        top_addr  = &node->top;
        top       = node->top;
        rgn       = MEM_TO_REGION(node); // The current region is the new one
        committed = REGION_BYTES_COMMITTED(rgn);
    }

    LOG_DEBUG("Arena", "Region extend committed: %zu", committed);
    // split top putting the remainder in top, and set the chunk that was top to in-use.
    FreeChunk *remainder = free_chunk_split(top, size, file, line);
    if (remainder == NULL) {
        remainder = (FreeChunk *)CHUNK_NEXT(top); // the sentinel
    }
    *top_addr = remainder;

    free_chunk_set_inuse(top);

    // update footprints
    arena->footprint += committed;
    arena->max_footprint = MAX(arena->footprint, arena->max_footprint);
    mem                  = CHUNK_TO_MEMORY(top);

    // FIXME: we called integrity checks here, but they don't yet deal with multiple
    // regions ARENA_CHECK_TOP_CHUNK_FILE_LINE(arena, file, line);
    // ARENA_CHECK_ALLOCATED_CHUNK(arena, (Chunk *)top, size, file, line);

    return mem;
}

/**
 * @brief Allocate a chunk from a large bin that is large enough to satisfy the request
 * and return its address.
 *
 * @param arena the Arena from which the chunk will be allocated.
 * @param request the number of bytes to allocate
 * @return the address of a chunk of memory or NULL if there are no more large chunks.
 */
static Chunk *allocate_from_large_bin(Arena *arena, size_t request, char const *file,
                                      int line) {
    DigitalSearchTree  *dst  = NULL;
    DigitalSearchTree **bin  = NULL;
    DigitalSearchTree  *root = NULL;
    size_t              size = CHUNK_SIZE_FROM_REQUEST(request);
    u32                 index;
    flag64              left_shift;
    Chunk              *chunk = NULL;

    if (arena->fast != NULL && arena->fast_size >= size) {
        // prefer fast node
        chunk = chunk_from_fast(arena, size, file, line);
    } else {
        INDEX_BY_VALUE64(size, ARENA_LARGE_BIN_COUNT, ARENA_LOG2_MIN_LARGE_CHUNK, index);
        if (ARENA_LARGE_MAP_IS_MARKED(arena, index)) {
            // search for the best fit from this bin
            bin        = ARENA_LARGE_BIN_AT(arena, index);
            root       = *bin;
            left_shift = ARENA_LEFT_SHIFT(index);
            dst        = dst_find_smallest_large_node(root, size, left_shift);
        }

        // If we didn't find a suitable chunk in the bin, select the next non-empty bin.
        if (dst == NULL) { // set root to next non-empty large bin
            flag64 left_bits = BIT_MASK_LEFT(IDX_TO_BIT64(index)) & arena->large_map;
            if (left_bits != 0) {
                flag64 least_bit = BIT_LSB(left_bits);
                COMPUTE_LSB2IDX64(least_bit, index);
                bin        = ARENA_LARGE_BIN_AT(arena, index);
                root       = *bin;
                left_shift = ARENA_LEFT_SHIFT(index);
                // We have a non-empty bin, so find the smallest node in that tree.
                dst = dst_find_smallest_large_node(root, size, left_shift);
            }
        }

        if (dst != NULL) {
            // we have a candidate chunk
            DigitalSearchTree *replacement = dst_remove_leftmost(dst);
            if (dst == *bin) {
                *bin = replacement;
                if (replacement == NULL) {
                    ARENA_CLEAR_LARGE_MAP(arena, index);
                }
            }

            // split victim if the remainder is large enough, otherwise, use it as is.
            FreeChunk *remainder
                = free_chunk_split(ARENA_DST_TO_FREE_CHUNK(dst), size, file, line);
            free_chunk_set_inuse(ARENA_DST_TO_FREE_CHUNK(dst));
            if (remainder != NULL) {
                // place remainder in the appropriate small or large bin
                if (CHUNK_SIZE(remainder) < ARENA_MIN_LARGE_CHUNK) {
                    insert_small_chunk(arena, remainder, file, line);
                } else {
                    DigitalSearchTree *tree_rem = ARENA_CHUNK_TO_DST(remainder);
                    dst_init_minimal(tree_rem);
                    insert_large_chunk(arena, tree_rem, file, line);
                }
            }
            chunk = (Chunk *)ARENA_DST_TO_FREE_CHUNK(dst);
        }
    }

    return chunk;
}

/**
 * @brief Find which region node (if any) owns the given address.
 *
 * This helper function searches the arena's region_list to find the RegionNode that
 * contains the given address. It returns NULL if the address is in the primary region
 * or not found in any region node.
 *
 * @param arena the arena whose region list will be searched
 * @param addr the address to search for
 * @return pointer to the RegionNode that owns addr, or NULL if addr is in primary region
 */
static RegionNode *find_region_node_owner(Arena *arena, void const *addr) {
    if (DLIST_IS_EMPTY(&arena->region_list)) {
        return NULL;
    }

    DList *entry = DLIST_NEXT(&arena->region_list);
    while (entry != &arena->region_list) {
        RegionNode *node = FL_CONTAINER_OF(entry, RegionNode, link);
        if (region_node_is_member(node, addr)) {
            return node;
        }
        entry = DLIST_NEXT(entry);
    }

    return NULL;
}

void *arena_aligned_alloc_throw(Arena *arena, size_t alignment, size_t size,
                                char const *file, int line) {
    FL_ASSERT_DETAILS_FILE_LINE(alignment != 0 && (alignment & (alignment - 1)) == 0,
                                "alignment %zu is not a power of 2", file, line,
                                alignment);

    if (alignment <= CHUNK_ALIGNMENT) {
        return arena_malloc_throw(arena, size, file, line);
    }

    FL_ASSERT_DETAILS_FILE_LINE(!CHUNK_REQUEST_OUT_OF_RANGE(alignment),
                                "alignment %zu exceeds maximum %zu", file, line,
                                alignment, CHUNK_MAX_REQUEST);
    FL_ASSERT_DETAILS_FILE_LINE(
        size <= CHUNK_MAX_REQUEST - 2 * alignment,
        "allocation request %zu with alignment %zu exceeds maximum", file, line, size,
        alignment);

    void     *raw          = arena_malloc_throw(arena, size + 2 * alignment, file, line);
    uintptr_t raw_addr     = (uintptr_t)raw;
    uintptr_t aligned_addr = ALIGN_UP(raw_addr, alignment);
    size_t    gap          = (size_t)(aligned_addr - raw_addr);

    if (gap > 0 && gap < CHUNK_MIN_SIZE) {
        aligned_addr += alignment;
        gap += alignment;
    }

    if (gap == 0) {
        return raw;
    }

    Chunk *raw_ch     = (Chunk *)((char *)raw - CHUNK_ALIGNED_SIZE);
    size_t total_size = CHUNK_SIZE(raw_ch);
    size_t flags      = raw_ch->size & CHUNK_INUSE_FLAGS;

    raw_ch->size      = gap | flags;
    Chunk *aligned_ch = (Chunk *)((char *)raw_ch + gap);
    aligned_ch->size  = (total_size - gap) | CHUNK_INUSE_FLAGS;

    arena_free_throw(arena, raw, file, line);

    return (void *)aligned_addr;
}

void *arena_calloc_throw(Arena *arena, size_t count, size_t size, char const *file,
                         int line) {
    if (count == 0) {
        count = 1;
    }
    if (size == 0) {
        size = 1;
    }

    FL_ASSERT_DETAILS_FILE_LINE(!CHUNK_PRODUCT_OUT_OF_RANGE(count, size),
                                "count %zu and size %zu exceed maximum %zu", file, line,
                                count, size, CHUNK_MAX_REQUEST);

    size_t request = count * size;
    void  *mem     = arena_malloc_throw(arena, request, file, line);
    memset(mem, 0, count * size);

    return mem;
}

void arena_free_throw(Arena *arena, void *mem, char const *file, int line) {
    FreeChunk *ch = FREE_CHUNK_FROM_MEMORY(mem, file, line);

    // TODO: instead of throwing fl_internal_error, throw fl_invalid_address. This will
    // probably require a new macro that works similarly to ARENA_RTCHECK, but is focused
    // on memory addresses. Ensure this chunk is at a valid memory address and it is in
    // use. It might also require changes to the dbg_check_* functions to throw
    // fl_invalid_address instead of an assertion failure, or another set of inline
    // static functions to do that.
    ARENA_RTCHECK(arena_address_ok(arena, ch), file, line);
    ARENA_CHECK_INUSE_CHUNK_FILE_LINE(arena, (Chunk *)ch, file, line);

    Chunk *next = CHUNK_NEXT(ch);

    size_t size           = CHUNK_SIZE(ch);
    bool   previous_inuse = CHUNK_IS_PREVIOUS_INUSE(ch);
    free_chunk_init(ch, size, previous_inuse);
    CHUNK_CLEAR_PREVIOUS_INUSE(next);
    if (!previous_inuse) {
        FreeChunk *prev      = chunk_get_previous((Chunk *)ch);
        size_t     prev_size = CHUNK_SIZE(prev);

        // merge backward. Note that prev could be the fast node, or in a small or large
        // bin. Handle it accordingly.
        //
        // - fast node: merge in place
        // - small or large bin: remove, merge, handle possible forward merge, insert if
        //   the final merged block is not the top block.
        size += prev_size;
        ARENA_RTCHECK(arena_address_ok(arena, prev), file, line);

        if (prev == arena->fast) {
            // The fast node is being absorbed by a backward merge. Clear it so it
            // won't be double-tracked (in fast AND in a bin after insertion below).
            arena->fast      = NULL;
            arena->fast_size = 0;
        } else if (ARENA_IS_CHUNK_SMALL(prev)) {
            // remove from small bin and merge below
            unlink_small_chunk(arena, prev, prev_size, file, line);
            ARENA_RTCHECK(free_chunk_previous_sibling(prev) == prev, file, line);
            ARENA_RTCHECK(free_chunk_next_sibling(prev) == prev, file, line);
        } else {
            // remove previous chunk from large bin - chunks will be merged below
            DigitalSearchTree *dst = ARENA_CHUNK_TO_DST(prev);
            u32 index;
            INDEX_BY_VALUE64(CHUNK_SIZE(dst), ARENA_LARGE_BIN_COUNT,
                             ARENA_LOG2_MIN_LARGE_CHUNK, index);
            DigitalSearchTree **bin = ARENA_LARGE_BIN_AT(arena, index);

            if (*bin != dst && DST_IS_ROOT(dst)) {
                // prev is a sibling of a tree node (self-referential parent/child,
                // but its siblings DList is linked into another node's list).
                // Just remove it from the sibling list; the bin root is unchanged.
                DLIST_REMOVE(&dst->siblings);
            } else {
                // prev is either the bin root or an internal tree node.
                DigitalSearchTree *replacement = dst_remove_leftmost(dst);
                if (*bin == dst) {
                    *bin = replacement;
                    if (replacement == NULL) {
                        ARENA_CLEAR_LARGE_MAP(arena, index);
                    }
                }
            }
            ARENA_RTCHECK(DST_IS_ROOT(dst), file, line);
            ARENA_RTCHECK(DST_IS_LEAF(dst), file, line);
            ARENA_RTCHECK(!DST_HAS_SIBLINGS(dst), file, line);
        }

        // regardless of where prev is located, merge it with ch
        free_chunk_merge_next(prev);

        // ensure the merge chunk is correctly sized
        ARENA_RTCHECK(CHUNK_SIZE(prev) == size, file, line);
        ARENA_RTCHECK(CHUNK_NEXT(prev) == next, file, line);
        ARENA_RTCHECK(chunk_previous_size(CHUNK_NEXT(prev)) == size, file, line);

        // the previous chunk is now the current chunk
        ch = prev;
    }

    if ((FreeChunk *)next == arena->top) {
        // Merge with the primary top. This handles both the normal case (top is a free
        // chunk) and the exhausted-top case (arena->top is the sentinel, which has
        // CHUNK_CURRENT_INUSE_FLAG set). In the sentinel case, the
        // CHUNK_CLEAR_PREVIOUS_INUSE(next) call above cleared sentinel's PREV_INUSE bit;
        // making ch the new top restores the invariant because ch->PREV_INUSE is already
        // set correctly by free_chunk_init. The sentinel itself stays in place as the
        // boundary marker with its PREV_INUSE correctly reflecting the free top before it.
        arena->top = ch;
        if (!CHUNK_IS_INUSE(next)) {
            // Normal case: next is a free top chunk; absorb it into ch.
            free_chunk_merge_next(ch);
        }
        // ch is the new top, so pull it off the fast node if it was there
        if (ch == arena->fast) {
            arena->fast      = NULL;
            arena->fast_size = 0;
        }

        // Returning, because there's no need to insert the chunk into a bin.
        arena->allocations--;
        return;
    }

    if (!CHUNK_IS_INUSE(next)) {
        // merge forward. The next chunk can be a region node top, fast node, or in a bin.
        {
            // Check if next chunk is a region node's top
            RegionNode *owner_node = find_region_node_owner(arena, ch);
            if (owner_node != NULL && (FreeChunk *)next == owner_node->top) {
                // merge with region node's top and return
                owner_node->top = ch;
                free_chunk_merge_next(ch);
                // ch is the new region node top, so pull it off the fast node if it was
                // there
                if (ch == arena->fast) {
                    arena->fast      = NULL;
                    arena->fast_size = 0;
                }

                // Returning, because there's no need to insert the chunk into a bin.
                arena->allocations--;
                return;
            }
        }

        if ((FreeChunk *)next == arena->fast) {
            // merge with fast node and return
            size_t fast_size = arena->fast_size + size;

            // top should not follow the fast node, because it should have already been
            // merged
            FreeChunk *fast_next = FREE_CHUNK_NEXT(arena->fast);
            ARENA_RTCHECK(fast_next != arena->top, file, line);

            free_chunk_merge_next(ch);
            arena->fast_size = fast_size;
            arena->fast      = ch;
            arena->allocations--;
            return; // there's no need to insert the chunk into a bin.
        } else {
            // merge forward with a node in a bin
            size_t next_size = CHUNK_SIZE(next);
            size += next_size;
            // pluck next from its bin
            if (next_size <= ARENA_MAX_SMALL_CHUNK) {
                unlink_small_chunk(arena, (FreeChunk *)next, next_size, file, line);
            } else {
                DigitalSearchTree *tree_next = ARENA_CHUNK_TO_DST(next);
                u32                fwd_index;
                INDEX_BY_VALUE64(CHUNK_SIZE(tree_next), ARENA_LARGE_BIN_COUNT,
                                 ARENA_LOG2_MIN_LARGE_CHUNK, fwd_index);
                DigitalSearchTree **fwd_bin = ARENA_LARGE_BIN_AT(arena, fwd_index);

                if (*fwd_bin != tree_next && DST_IS_ROOT(tree_next)) {
                    // tree_next is a sibling of a tree node (self-referential
                    // parent/child, but its siblings DList is linked into another
                    // node's list). Just remove it from the sibling list; the bin
                    // root is unchanged.
                    DLIST_REMOVE(&tree_next->siblings);
                } else {
                    DigitalSearchTree *replacement2 = dst_remove_leftmost(tree_next);
                    if (*fwd_bin == tree_next) {
                        *fwd_bin = replacement2;
                        if (replacement2 == NULL) {
                            ARENA_CLEAR_LARGE_MAP(arena, fwd_index);
                        }
                    }
                }
                ARENA_RTCHECK(DST_IS_ROOT(tree_next), file, line);
                ARENA_RTCHECK(DST_IS_LEAF(tree_next), file, line);
                ARENA_RTCHECK(!DST_HAS_SIBLINGS(tree_next), file, line);
            }

            // extend chunk into next
            size_t original_size = CHUNK_SIZE(ch);
            free_chunk_merge_next(ch);
            ARENA_RTCHECK(CHUNK_SIZE(ch) == size, file, line);
            if (original_size < ARENA_MIN_LARGE_CHUNK
                && CHUNK_SIZE(ch) > ARENA_MAX_SMALL_CHUNK) {
                // small chunk has just been promoted to a large chunk, so complete its
                // initialization
                DigitalSearchTree *tc = ARENA_CHUNK_TO_DST(ch);
                dst_init_minimal(tc);
            }
        }
    }

    /*
     * At this point, the chunk may or may not have been merged backward or forward. It
     * was NOT merged with top or the fast node, so it needs to be placed in a bin, or
     * replace the fast node.
     */
    if (size <= ARENA_MAX_SMALL_CHUNK) {
        insert_small_chunk(arena, ch, file, line);
        // don't display siblings, because they could be the head of the bin
    } else {
        DigitalSearchTree *tree = ARENA_CHUNK_TO_DST(ch);
        dst_init_minimal(tree);
        insert_large_chunk(arena, tree, file, line);
    }

    FL_ASSERT_DETAILS(CHUNK_SIZE(ch) == free_chunk_footer_size(ch),
                      "Expected 0x%04zx. Actual 0x%04zx", CHUNK_SIZE(ch),
                      free_chunk_footer_size(ch));

    arena->allocations--;
}

void arena_free_pointer_throw(Arena *arena, void **ptr, char const *file, int line) {
    FL_ASSERT_NOT_NULL(ptr);
    void *mem = *ptr;
    arena_free_throw(arena, mem, file, line);
    *ptr = 0;
}

void *arena_malloc_throw(Arena *arena, size_t request, char const *file, int line) {
    size_t size = CHUNK_SIZE_FROM_REQUEST(request);
    Chunk *ch   = NULL;

    LOG_DEBUG("Arena", "request %zu, %s:%d", request, file, line);
    ARENA_CHECK_TOP_CHUNK_FILE_LINE(arena, file, line);
    if (request <= ARENA_MAX_SMALL_REQUEST) {
        LOG_DEBUG("Arena", "small %zu, %s:%d", request, file, line);
        // Prefer a "remainderless fit", i.e., a small bin with a chunk large enough to
        // satisfy the request, but too small to split.
        if (ARENA_HAS_REMAINDERLESS_SMALL_BIN(arena, size)) {
            // 1. Remainderless, exact fit.
            ch = allocate_exact_fit(arena, size, file, line);
            ARENA_CHECK_ALLOCATED_CHUNK(arena, ch, size, file, line);
        } else if (size <= arena->fast_size) {
            // 2. Allocate from the fast node, splitting it if possible.
            ch = chunk_from_fast(arena, size, file, line);
        } else if (ARENA_SMALL_BITS(arena, size) != 0) {
            // 3. Use the chunk from the next non-empty smallbin, split it, and place the
            // remainder on the fast node.
            ch = allocate_from_next_nonempty_small_bin(arena, size, file, line);
        } else if (arena->large_map != 0) {
            // 4. No small bins and fast node is too small. Try a large free chunk.
            ch = allocate_small_request_from_large_bin(arena, size, file, line);
        }
    } else {
        LOG_DEBUG("Arena", "large %zu, %s:%d", request, file, line);
        ch = allocate_from_large_bin(arena, request, file, line);
        if (ch != NULL) {
            ARENA_CHECK_ALLOCATED_CHUNK(arena, ch, size, file, line);
        }
    }

    void *mem = NULL;
    if (ch == NULL) {
        LOG_DEBUG("Arena", "Try top");
        // didn't allocate from a bin or the fast node, so try top
        ARENA_CHECK_TOP_CHUNK_FILE_LINE(arena, file, line);
        size_t top_size = CHUNK_SIZE(arena->top);

        if (top_size >= size) {
            LOG_DEBUG("Arena", "Allocate from top");
            // 5. Allocate from top.
            ch = allocate_from_top(arena, size, file, line);
            ARENA_CHECK_ALLOCATED_CHUNK(arena, ch, size, file, line);
            mem = CHUNK_TO_MEMORY(ch);
        } else {
            if (!DLIST_IS_EMPTY(&arena->region_list)) {
                DList      *entry = &arena->region_list;
                RegionNode *rn    = NULL;
                while (entry != &arena->region_list) {
                    rn = FL_CONTAINER_OF(entry, RegionNode, link);
                    if (CHUNK_SIZE(rn->top) >= size) {
                        break;
                    }
                    rn    = NULL;
                    entry = DLIST_NEXT(entry);
                }

                if (rn != NULL) {
                    // allocate from this region node
                    ch  = region_node_allocate(rn, size, file, line);
                    mem = CHUNK_TO_MEMORY(ch);
                }
            }

            if (mem == NULL) {
                LOG_DEBUG("Arena", "Extend");
                // 6. Try to extend the current Region by either committing more memory
                // from
                //    what's already reserved, or extending reserved memory and
                //    committing from that.
                mem = extend(arena, request, file, line);
                ch  = CHUNK_FROM_MEMORY_FILE_LINE(mem, file, line);
            }
        }
    } else {
        mem = CHUNK_TO_MEMORY(ch);
    }
    CHUNK_VALIDATE(ch, size);
    arena->allocations++;
    return mem;
}

void *arena_realloc_throw(Arena *arena, void *mem, size_t size, char const *file,
                          int line) {
    FL_ASSERT_DETAILS_FILE_LINE(!CHUNK_REQUEST_OUT_OF_RANGE(size),
                                "allocation request %zu exceeds maximum %zu", file, line,
                                size, CHUNK_MAX_REQUEST);

    void *p;
    if (mem == NULL) {
        p = arena_malloc_throw(arena, size, file, line);
    } else {
        Chunk *ch      = CHUNK_FROM_MEMORY_FILE_LINE(mem, file, line);
        size_t payload = CHUNK_PAYLOAD_SIZE(ch);
        p              = arena_malloc_throw(arena, size, file, line);
        size_t n       = (size < payload) ? size : payload;
        (void)memcpy(p, mem, n);
        arena_free_throw(arena, mem, file, line);
    }

    return p;
}

// ============================================================================
// Mid-Level Arena Inspection APIs
// ============================================================================

size_t arena_footprint(Arena const *arena) {
    return arena->footprint;
}

size_t arena_max_footprint(Arena const *arena) {
    return arena->max_footprint;
}

size_t arena_allocation_count(Arena const *arena) {
    return arena->allocations;
}

size_t arena_allocation_size(void *mem) {
    if (mem == NULL) {
        return 0;
    }
    // Get chunk header (reverse of CHUNK_TO_MEMORY)
    Chunk *ch = (Chunk *)(((char *)mem) - CHUNK_ALIGNED_SIZE);
    return CHUNK_SIZE(ch);
}

bool arena_is_valid_allocation(Arena *arena, void const *mem) {
    return arena_address_ok(arena, mem);
}
