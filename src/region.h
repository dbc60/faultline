#ifndef FAULTLINE_REGION_H_
#define FAULTLINE_REGION_H_

/**
 * @file region.h
 * @author Douglas Cuthbertson
 * @brief Definitions for a Region of memory.
 * @version 0.1
 * @date 2022-02-12
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // for u32
#include <faultline/fl_exception_types.h>   // for FLExceptionReason
#include <faultline/fl_threads.h>           // for mtx_t
#include <faultline/size.h>                 // for TWO_SIZE_T_SIZES
#include <stddef.h>               // for size_t
#include "atomic.h"               // for AtomicCharPtr
#include "bits.h"                 // for ALIGN_UP

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief The number of bytes to which offsets and allocations must be aligned. The
 * default is 16 bytes on a 64-bit OS.
 */
#define REGION_ALIGNMENT TWO_SIZE_T_SIZES

/**
 * @brief The size of a Region rounded up to a multiple of REGION_ALIGNMENT.
 */
#define REGION_ALIGNED_SIZE ALIGN_UP(sizeof(Region), REGION_ALIGNMENT)

/**
 * @brief The number of bytes committed in a Region.
 */
#define REGION_BYTES_COMMITTED(region) \
    (size_t)((region)->end_committed - (char *)(region))

/**
 * @brief The number of bytes reserved in a Region.
 */
#define REGION_BYTES_RESERVED(region) (size_t)((region)->end_reserved - (char *)(region))

/// Convert a Region pointer to the first usable byte after the Region header.
#define REGION_TO_MEM(RG) (void *)((char *)(RG) + REGION_ALIGNED_SIZE)
/// Convert a memory pointer (from REGION_TO_MEM) back to its owning Region.
#define MEM_TO_REGION(MEM) ((Region *)((char *)(MEM) - REGION_ALIGNED_SIZE))

/// Thrown when a Region cannot allocate or extend memory.
extern FLExceptionReason region_out_of_memory;
/// Thrown when new_custom_region detects invalid parameters (overflow, page >
/// granularity).
extern FLExceptionReason region_initialization_failure;

/**
 * @brief A Region tracks the amount of memory that is committed and reserved in a memory
 * block. The initial block is a multiple of the system granularity sufficient to contain
 * the Region struct itself and any additional memory requested when new_region is
 * called.
 */
struct Region {
    mtx_t lock;
    /** The end of reserved memory. */
    AtomicCharPtr end_reserved;
    /** The end of committed memory. */
    AtomicCharPtr end_committed;
    /** The size of a reservation is a multiple of this. */
    u32 granularity;
    /** The size of a commit is a multiple of this. */
    u32 page_size;
};
typedef struct Region Region;

/**
 * @brief Allocate a new Region in a block of memory large enough to contain the Region,
 * the requested number of bytes, and additional reserved blocks of memory.
 *
 * If the value of granularity_multiplier > 1, then the size of each reserved block is
 * the value of the system granularity multiplied by this multiplier, otherwise they are
 * each the size of the system granularity.
 *
 * Similarly, if page_size_multiplier > 1, then the size of committed memory is rounded
 * up to a multiple of the system page size multiplied by this multiplier, otherwise
 * committed memory is a multiple the system page size.
 *
 * @param request The number of bytes to allocate from the region.
 * @param blocks The number of additional blocks of memory to reserve when creating this
 * region. The size of a block is the system granularity in bytes multiplied by the
 * granularity multiplier.
 * @param granularity_multiplier a multiplier to apply to all memory reservation requests
 * when attempting to expand this region. If zero or one, then the system granularity is
 * applied.
 * @param page_size_multiplier a multiplier to apply to all memory commit requests when
 * allocating from reserved memory. If zero or one, then the system page-size is applied.
 * @return The address of the first aligned-byte after the address of the newly allocated
 * Region.
 * @throw region_out_of_memory if a new Region cannot be allocated.
 * @throw region_initialization_failure is thrown if granularity_multiplier is so large
 * as to cause the applied reservation size to wrap.
 * @throw region_initialization_failure is thrown if page_size_multiplier is so large as
 * to cause the applied commit size to wrap.
 * @throw region_initialization_failure if the applied page-size is greater than the
 * applied granularity.
 */
Region *new_custom_region(size_t request, u32 blocks, u32 granularity_multiplier,
                          u32 page_size_multiplier);

/**
 * @brief Allocate a new Region in a block of memory large enough to contain the Region,
 * the requested number of bytes, and additional reserved blocks of memory.
 *
 * @param commit The number of bytes to allocate from the region.
 * @param reserve The number of additional blocks of memory to reserve when creating this
 * region. The size of a block is the system granularity in bytes.
 * @return The address of a newly allocated Region.
 * @throw region_out_of_memory if a new Region cannot be allocated.
 */
Region *new_region(size_t commit, u32 reserve);

/**
 * @brief Free all memory associated with a region.
 *
 * @param region the address of the Region to be released.
 * @throw fl_internal_error if the underlying platform free fails.
 */
void release_region(Region *region);

/**
 * @brief get the number of bytes of memory available for extending a region's reserved
 * memory.
 *
 * @param region the address of a Region.
 * @return the number of bytes by which a region's reserved memory can be expanded.
 */
size_t region_available_bytes(Region const *region);

/**
 * @brief commit an additional request number of bytes, extending the region as needed.
 *
 * The request will be rounded up to the next multiple of a page size.
 *
 * @param region the address of the Region to extend.
 * @param request the number of additional bytes to commit.
 * @return the number of newly-committed bytes.
 * @throw region_out_of_memory if the region cannot be extended.
 */
size_t region_extend(Region *region, size_t request);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_REGION_H_
