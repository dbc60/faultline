#ifndef REGION_NODE_H_
#define REGION_NODE_H_

/**
 * @file region_node.h
 * @author Douglas Cuthbertson
 * @brief RegionNode: a chunk-managed memory block linked into an Arena's overflow list.
 * @version 0.1
 * @date 2025-09-26
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/dlist.h>    // for DList
#include <faultline/size.h>                 // for TWO_SIZE_T_SIZES
#include <stdbool.h>              // for bool
#include <stddef.h>               // for ptrdiff_t, size_t
#include "bits.h"                 // for ALIGN_UP
#include "chunk.h"                // for Chunk, FreeChunk
#include <faultline/fl_abbreviated_types.h> // for u32
#include <faultline/fl_exception_types.h>   // for FLExceptionReason
#include "region.h"               // for release_region, MEM_TO_REGION

#if defined(__cplusplus)
extern "C" {
#endif

#define REGION_NODE_ALIGNED_SIZE ALIGN_UP(sizeof(RegionNode), TWO_SIZE_T_SIZES)

extern FLExceptionReason region_node_out_of_memory;

/**
 * @brief Allow for an Arena to expand its memory footprint beyond a single, contiguous
 * block of memory. A RegionNode is embedded in a Region, just like an Arena is, so use
 * MEM_TO_REGION(region_node) when converting from a RegionNode to a Region.
 */
typedef struct RegionNode {
    DList      link; ///< DList linkage for overflow_regions
    Chunk     *base; ///< the initial address of top
    FreeChunk *top;  ///< the address of the remaining unallocated memory
} RegionNode;

/**
 * @brief Allocate a new block of memory large enough to contain a RegionNode, the commit
 * request and the given number of reserved blocks (each the size of the system
 * granularity).
 *
 * @param commit the number of bytes to commit beyond the size of the region node. This
 * value will be added to the aligned size of the region node and then rounded up to the
 * system granularity.
 * @param reserve the number of additional blocks of memory that will be reserved, where
 * the size of each block is the system granularity.
 * @return the address of a newly allocated RegionNode.
 * @throw region_node_out_of_memory when memory cannot be allocated.
 */
RegionNode *new_region_node(size_t commit, u32 reserve);

static inline void release_region_node(RegionNode *node) {
    release_region(MEM_TO_REGION(node));
}

/**
 * @brief allocate from a region node
 *
 * @param node the node from which memory will be allocated
 * @param size the number of bytes to allocate
 * @param file a pointer to the file name
 * @param line the line number
 * @return Chunk*
 * @throw fl_internal_error if there's insufficient space, because it was already
 * checked for sufficient space
 */
Chunk *region_node_allocate(RegionNode *node, size_t size, char const *file, int line);

void region_node_release(RegionNode *node, void *mem, char const *file, int line);

/**
 * @brief Check if a memory address belongs to this region node
 *
 * @param node the region node to check
 * @param mem the memory address to test
 * @return true if mem is within the allocatable range of this node, false otherwise
 */
static inline bool region_node_is_member(RegionNode const *node, void const *mem) {
    ptrdiff_t address = (ptrdiff_t)mem;
    // Check if address is within the allocatable range [base, top)
    // Allocated chunks are before top, and top points to the remaining free space
    return address >= (ptrdiff_t)node->base && address < (ptrdiff_t)node->top;
}

#if defined(__cplusplus)
}
#endif

#endif // REGION_NODE_H_
