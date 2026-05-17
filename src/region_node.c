/**
 * @file region_node.c
 * @author Douglas Cuthbertson
 * @brief RegionNode allocation, release, and membership operations.
 * @version 0.1
 * @date 2025-09-26
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "region_node.h"
#include <faultline/dlist.h>                // for DLIST_INIT
#include <faultline/fl_exception_service.h> // for fl_internal_error, fl_in...
#include <faultline/fl_exception_types.h>   // for FLExceptionReason
#include <faultline/fl_log.h>               // for LOG_ERROR
#include <faultline/fl_try.h>               // for FL_THROW_DETAILS_FILE_LINE
#include <faultline/size.h>                 // for TWO_SIZE_T_SIZES
#include <stdbool.h>                        // for true
#include <stddef.h>                         // for size_t, NULL
#include "bits.h"                           // for ALIGN_UP
#include <faultline/fl_abbreviated_types.h> // for u32

FLExceptionReason region_node_out_of_memory = "failed to allocate a RegionNode";

RegionNode *new_region_node(size_t commit, u32 reserve) {
    size_t  commit_size = REGION_NODE_ALIGNED_SIZE + ALIGN_UP(commit, TWO_SIZE_T_SIZES)
                          + CHUNK_SENTINEL_SIZE;
    Region *region      = new_region(commit_size, reserve);

    RegionNode *rl = REGION_TO_MEM(region);
    DLIST_INIT(&rl->link);
    rl->base               = (Chunk *)(((char *)rl) + REGION_NODE_ALIGNED_SIZE);
    rl->top                = (FreeChunk *)rl->base;
    size_t const committed = REGION_BYTES_COMMITTED(region);
    size_t       top_size  = committed - REGION_ALIGNED_SIZE - REGION_NODE_ALIGNED_SIZE
                             - CHUNK_SENTINEL_SIZE;

    CHUNK_SET_SENTINEL(rl->top);

    free_chunk_init(rl->top, top_size, true);
    // Ensure the sentinel is never consolidated with top by setting its in-use bit. Note
    // the size is set to zero to indicate that the arena in which this region node is
    // contained should attempt to extend itself the next time it can't satisfy an
    // allocation request from top, the fast chunk or its free bins.
    CHUNK_SET_INUSE(CHUNK_NEXT(rl->top));

    return rl;
}

Chunk *region_node_allocate(RegionNode *node, size_t size, char const *file, int line) {
    // Verify we have sufficient space
    if (CHUNK_SIZE(node->top) < size) {
        LOG_ERROR("Region Node",
                  "Insufficient space in region node: requested %zu, available %zu",
                  size, CHUNK_SIZE(node->top));
        FL_THROW_DETAILS_FILE_LINE(
            fl_internal_error,
            "Insufficient space in region node: requested %zu, available %zu", file,
            line, size, CHUNK_SIZE(node->top));
    }

    // Slice off a chunk from top
    Chunk     *ch        = (Chunk *)node->top;
    FreeChunk *remainder = free_chunk_split(node->top, size, file, line);

    // Update top to point to remainder or sentinel
    if (remainder == NULL) {
        node->top = (FreeChunk *)CHUNK_NEXT(node->top); // the sentinel
    } else {
        node->top = remainder;
    }

    // Mark the chunk as in-use
    free_chunk_set_inuse((FreeChunk *)ch);

    return ch;
}

void region_node_release(RegionNode *node, void *mem, char const *file, int line) {
    // Validate that the memory belongs to this node
    if (!region_node_is_member(node, mem)) {
        LOG_ERROR("Region Node",
                  "Memory address %p does not belong to this region node [%p, %p)", mem,
                  node->base, node->top);
        FL_THROW_DETAILS_FILE_LINE(
            fl_invalid_address,
            "Memory address %p does not belong to this region node [%p, %p)", file, line,
            mem, node->base, node->top);
    }

    // Note: The actual freeing logic (merging, binning) is handled by arena_free_throw.
    // This function just validates the address belongs to this node.
    // RegionNodes don't maintain their own free lists - they're managed by the Arena.
}
