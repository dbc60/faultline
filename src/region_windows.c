/**
 * @file region_windows.c
 * @author Douglas Cuthbertson
 * @brief The Windows-specific implementation of allocation functions for the
 * Region library.
 * @version 0.1
 * @date 2022-02-12
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h>        // for u32
#include <faultline/fl_exception_service_assert.h> // for FL_ASSERT*
#include <faultline/fl_threads.h>                  // for mtx_init, WIN32_LEAN_AND_MEAN
#include <faultline/fl_try.h>                      // for FL_THROW_DETAILS, FL_THROW
#include <stdatomic.h>                   // for atomic_fetch_add
#include <stddef.h>                      // for size_t, NULL, ptrdiff_t
#include "bits.h"                        // for ALIGN_UP, ALIGN_DOWN
#include <faultline/fl_exception_service.h>        // for fl_internal_error
#include "region.h"                      // for Region, region_initializati...
#include "win32_platform.h"              // for get_memory_info

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>        // IWYU pragma: keep - sets target architecture
#include <errhandlingapi.h> // for GetLastError
#include <memoryapi.h>      // for VirtualFree, VirtualAlloc
#include <minwindef.h>      // for MEM_RELEASE, MEMORY_BASIC_I...

Region *new_region(size_t commit, u32 reserve) {
    return new_custom_region(commit, reserve, 0, 0);
}

Region *new_custom_region(size_t commit, u32 reserve, u32 granularity_multiplier,
                          u32 page_size_multiplier) {
    char   *top;
    u32     granularity, page_size;
    u32     applied_granularity;
    u32     applied_page_size;
    size_t  to_reserve;
    size_t  to_commit;
    Region *region;

    get_memory_info(&granularity, &page_size);
    if (granularity_multiplier > 1) {
        applied_granularity = granularity * granularity_multiplier;
        // detect unsigned overflow
        if (applied_granularity < granularity) {
            FL_THROW_DETAILS(region_initialization_failure,
                             "Granularity overflow - expected applied granularity >= "
                             "granularity, actual %u < %u",
                             applied_granularity, granularity);
        }
    } else {
        applied_granularity = granularity;
    }

    if (page_size_multiplier > 1) {
        applied_page_size = page_size * page_size_multiplier;
        // detect unsigned overflow
        if (applied_page_size < page_size) {
            FL_THROW_DETAILS(region_initialization_failure,
                             "Page size overflow - expected applied page size >= "
                             "page size, actual %u < %u",
                             applied_page_size, page_size);
        }
    } else {
        applied_page_size = page_size;
    }

    // final page size must be no larger than the final granularity.
    if (applied_page_size > applied_granularity) {
        FL_THROW_DETAILS(region_initialization_failure,
                         "Page size >= granularity, actual %u < %u", applied_page_size,
                         applied_granularity);
    }

    // Round up the commit-request to the minimum alignment.
    commit = ALIGN_UP(commit, REGION_ALIGNMENT);
    // add space for an aligned Region and round up to a multiple of the page size
    to_commit = ALIGN_UP(commit + REGION_ALIGNED_SIZE, applied_page_size);
    // reserve the reservation request plus the commit request rounded up to a multiple
    // of the granularity.
    to_reserve = (size_t)reserve * (size_t)applied_granularity
                 + ALIGN_UP(to_commit, applied_granularity);
    top = VirtualAlloc(0, to_reserve, MEM_RESERVE, PAGE_NOACCESS);
    FL_ASSERT_NOT_NULL(top);

    region = (Region *)(top);

    // Commit the request.
    void *mem = VirtualAlloc(top, to_commit, MEM_COMMIT, PAGE_READWRITE);
    if (mem == NULL || mem != top) {
        VirtualFree(top, 0, MEM_RELEASE);
        FL_THROW(region_initialization_failure);
    }

    mtx_init(&region->lock, mtx_plain);
    region->end_reserved  = top + to_reserve;
    region->end_committed = top + to_commit;
    region->granularity   = applied_granularity;
    region->page_size     = applied_page_size;

    return region;
}

void release_region(Region *region) {
    FL_ASSERT_TRUE(VirtualFree(region, 0, MEM_RELEASE));
}

// Always called with the region lock held
size_t commit(Region *region, size_t to_commit) {
    // commit in multiples of a page size
    void *commit_address;

    void *end_committed = region->end_committed;
    commit_address = VirtualAlloc(end_committed, to_commit, MEM_COMMIT, PAGE_READWRITE);

    // VirtualAlloc shouldn't fail very often, because we've been careful about ensuring
    // there is sufficient reserved memory to allow for the commit.
    FL_ASSERT_DETAILS(commit_address != NULL,
                      "Error %d: region 0x%p failed to allocate %zu bytes at 0x%p; "
                      "reserved(%zu), committed(%zu)",
                      GetLastError(), region, to_commit, end_committed,
                      REGION_BYTES_RESERVED(region), REGION_BYTES_COMMITTED(region));

    /* Check returned pointer for consistency */
    FL_ASSERT_DETAILS(commit_address == region->end_committed,
                      "commit address(0x%p), top committed(0x%p), to-commit(%zu)",
                      commit_address, region->end_committed, to_commit);

    /* address must be on a page boundary */
    FL_ASSERT_ZERO_SIZE_T((size_t)commit_address % region->page_size);

    /* Adjust the regions commit top */
    atomic_fetch_add(&region->end_committed, (ptrdiff_t)to_commit);

    return to_commit;
}

// Always called while holding the region lock.
// decommit_size must be a multiple of the region's page size.
void decommit(Region *region, size_t decommit_size) {
    void *page = region->end_committed - decommit_size;

    FL_ASSERT_TRUE(VirtualFree(page, decommit_size, MEM_DECOMMIT));
    region->end_committed = page;
}

/**
 * @brief attempt to extend the size of the given region to satisfy a commit request.
 *
 * This function assumes to_commit is a multiple of the region's page size.
 *
 * 1. Commit as much of the remainder of the reserved region as needed.
 * 2. If we need need more:
 *    1. Reserve a multiple of the region granularity that's large
 *       enough to cover the commit.
 *    2. Commit "to_commit" bytes
 *
 * Always called with the region lock held. <== FIXME: check call sites or change this
 * function to lock first!!!
 *
 * @param region the Region to extend
 * @param to_commit the number of bytes to commit. This should be a multiple of a page
 * size
 * @return the number of bytes committed
 * @throw region_out_of_memory
 */
size_t extend_region(Region *region, size_t to_commit) {
    size_t can_commit    = region->end_reserved - region->end_committed;
    void  *base_reserved = NULL;
    size_t committed     = 0;

    if (can_commit > 0 && can_commit <= to_commit) {
        // commit the rest of the region's reserved memory
        committed = commit(region, can_commit);
        to_commit -= committed;
        // always commit a multiple of a page size
        to_commit = ALIGN_UP(to_commit, region->page_size);
    }

    if (to_commit > 0) {
        size_t available = region_available_bytes(region);
        if (available >= to_commit) {
            if ((size_t)(region->end_reserved - region->end_committed) < to_commit) {
                // extend reserved memory
                size_t to_reserve = ALIGN_UP(to_commit, region->granularity);
                base_reserved = VirtualAlloc((LPVOID)region->end_reserved, to_reserve,
                                             MEM_RESERVE, PAGE_NOACCESS);
                FL_ASSERT_NOT_NULL(base_reserved);

                // Verify that the returned pointer is the expected address
                if (base_reserved != region->end_reserved) {
                    VirtualFree(base_reserved, 0, MEM_RELEASE);
                    FL_THROW(fl_internal_error);
                }

                // Adjust the region's end reserved and reserved size
                atomic_fetch_add(&region->end_reserved, (ptrdiff_t)to_reserve);
            }

            // commit some multiple of a page size to satisfy the requirement
            committed += commit(region, to_commit);
        } else {
            // No! Can't extend the current region
            FL_THROW_DETAILS(region_out_of_memory, "available %zu, to-commit %zu",
                             available, to_commit);
        }
    }

    return committed;
}

// Always called with the region lock held
void shrink(Region *region, size_t release_size, size_t to_decommit) {
    MEMORY_BASIC_INFORMATION memory_info;
    size_t                   to_release;
    size_t                   last_reserved_size;
    char                    *end_reserved = region->end_reserved - 1;

    // Ensure we release on a system granularity boundary
    to_release = ALIGN_DOWN(release_size, region->granularity);

    VirtualQuery(end_reserved, &memory_info, sizeof memory_info);

    // Get the number of bytes reserved in this region
    last_reserved_size = end_reserved - (char *)memory_info.AllocationBase;

    /*
     * Release reserved memory regions leaving no less than system granularity
     * number of bytes reserved.
     */
    while (region != memory_info.AllocationBase && last_reserved_size <= to_release) {
        FL_ASSERT_TRUE(VirtualFree(memory_info.AllocationBase, 0, MEM_RELEASE));

        // Lather, rinse, and repeat
        region->end_reserved = memory_info.AllocationBase;
        end_reserved         = region->end_reserved - 1;
        to_release -= last_reserved_size;
        VirtualQuery(end_reserved, &memory_info, sizeof memory_info);
        last_reserved_size = end_reserved - (char *)memory_info.AllocationBase;
    }

    // Ensure committed memory pointer is within bounds of reserved memory
    if (region->end_committed > region->end_reserved) {
        to_decommit -= region->end_committed - region->end_reserved;
        region->end_committed = region->end_reserved;
    }

    if (to_decommit > 0) {
        decommit(region, to_decommit);
    }
}

size_t region_available_bytes(Region const *region) {
    MEMORY_BASIC_INFORMATION memory_info;
    size_t                   bytes;
    void                    *reserved = region->end_reserved;
    void                    *base;

    // bytes is the number of bytes written to memory_info
    bytes = VirtualQuery(reserved, &memory_info, sizeof memory_info);
    FL_ASSERT_DETAILS(bytes != 0,
                      "VirtualQuery failed to retrieve info on address 0x%p: %u",
                      reserved, GetLastError());

    base = memory_info.BaseAddress;
    // get the number of bytes after reserved memory
    if (base == reserved && memory_info.State == MEM_FREE) {
        bytes = memory_info.RegionSize;
    } else {
        bytes = 0;
    }

    // add in the number of bytes between what's committed and what's reserved
    bytes += (size_t)(region->end_reserved - region->end_committed);

    return bytes;
}
