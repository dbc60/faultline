/**
 * @file region_windows_test.c
 * @author Douglas Cuthbertson
 * @brief Test cases for the Windows-specific parts of the Region library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "region_test.h"
#include "region_windows.h" // region_can_extend
#include "win32_platform.h" // get_memory_info()

#include <faultline/fl_test.h>              // FLTestCase
#include <faultline/fl_exception_service.h> // fl_internal_error
#include <faultline/fl_exception_types.h>   // FLExceptionReason
#include <faultline/fl_abbreviated_types.h> // u32

#include <stddef.h> // size_t, NULL

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>


static void cleanup_region_allocation_windows(FLTestCase *data) {
    RegionAllocationTestCase *tc = FL_CONTAINER_OF(data, RegionAllocationTestCase, tc);
    if (tc->region != NULL) {
        release_region(tc->region);
        tc->region = NULL;
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Successful Reservation", RegionAllocationTestCase,
                           test_reserve, fl_default_setup, cleanup_region_allocation_windows) {
    MEMORY_BASIC_INFORMATION mem_info;
    u32                      expected_reserved;
    Region                  *region = new_region(0, 0);

    t->region = region;

    get_memory_info(&expected_reserved, 0);
    VirtualQuery(region, &mem_info, sizeof mem_info);
    FL_ASSERT_EQ_PTR(region, mem_info.BaseAddress);
    FL_ASSERT_EQ_SIZE_T((size_t)expected_reserved, REGION_BYTES_RESERVED(region));
}

FL_TYPE_TEST_SETUP_CLEANUP("Large Reservation", RegionAllocationTestCase,
                           test_absurd_reservation, fl_default_setup,
                           cleanup_region_allocation_windows) {
    size_t                   request = MEBI(8); // commit a modest-size chunk.
    u32                      granularity;
    MEMORY_BASIC_INFORMATION mem_info;

    get_memory_info(&granularity, 0);

    // As of 2023, 64 TEBI is the maximum amount of memory 64-bit Windows can
    // manage. We should be able to reserve it all.
    u32 additional_blocks = (u32)(TEBI(64) / granularity) - (u32)(request / granularity);
    Region *region        = new_region(request, additional_blocks);
    t->region             = region;

    VirtualQuery(region, &mem_info, sizeof mem_info);
    FL_ASSERT_EQ_PTR(region, mem_info.BaseAddress);
    FL_ASSERT_EQ_UINT32(granularity, region->granularity);

    size_t committed_bytes = region->end_committed - (char *)region;
    FL_ASSERT_GE_SIZE_T(committed_bytes, mem_info.RegionSize);
    FL_ASSERT_GT_SIZE_T(REGION_BYTES_RESERVED(region), (size_t)0);
}

// ---------------------------------------------------------------------------
// region_available_bytes
// ---------------------------------------------------------------------------

static void setup_with_extra_blocks(FLTestCase *data) {
    RegionAllocationTestCase *tc = FL_CONTAINER_OF(data, RegionAllocationTestCase, tc);
    tc->region                   = new_region(0, 4);
}

FL_TYPE_TEST_SETUP_CLEANUP("Available Bytes", RegionAllocationTestCase,
                           test_available_bytes, setup_with_extra_blocks,
                           cleanup_region_allocation_windows) {
    Region *region = t->region;

    size_t available            = region_available_bytes(region);
    size_t reserved_uncommitted = (size_t)(region->end_reserved - region->end_committed);
    size_t page_size            = region->page_size;

    // Available bytes must be at least the reserved-but-uncommitted portion
    FL_ASSERT_GE_SIZE_T(available, reserved_uncommitted);

    // After committing one page, available should decrease by exactly one page
    commit(region, page_size);
    FL_ASSERT_EQ_SIZE_T(available - page_size, region_available_bytes(region));

    // Restore
    decommit(region, page_size);
}

// ---------------------------------------------------------------------------
// region_can_extend
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("Can Extend", RegionAllocationTestCase, test_can_extend,
                           setup_with_extra_blocks, cleanup_region_allocation_windows) {
    Region                  *region = t->region;
    MEMORY_BASIC_INFORMATION mbi;

    // Query the state of memory immediately after the reservation
    SIZE_T result = VirtualQuery((LPVOID)region->end_reserved, &mbi, sizeof(mbi));
    FL_ASSERT_TRUE(result != 0);

    // Verify region_can_extend is consistent with the actual memory state
    bool can = region_can_extend(region, region->granularity);

    if (mbi.State == MEM_FREE && mbi.RegionSize >= region->granularity) {
        FL_ASSERT_TRUE(can);
    } else {
        FL_ASSERT_FALSE(can);
    }
}

// ---------------------------------------------------------------------------
// region_available_bytes after committing all reserved space
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("Available After Full Commit", RegionAllocationTestCase,
                           test_available_after_full_commit, setup_with_extra_blocks,
                           cleanup_region_allocation_windows) {
    Region *region      = t->region;
    size_t  uncommitted = (size_t)(region->end_reserved - region->end_committed);

    // Commit all remaining reserved-but-uncommitted space
    if (uncommitted > 0) {
        commit(region, uncommitted);
    }

    FL_ASSERT_EQ_PTR((void *)region->end_reserved, (void *)region->end_committed);

    // Available bytes should now only reflect free space beyond the reservation
    size_t available = region_available_bytes(region);

    // Verify against VirtualQuery directly
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = VirtualQuery((LPVOID)region->end_reserved, &mbi, sizeof(mbi));
    FL_ASSERT_TRUE(result != 0);

    size_t expected = 0;
    if (mbi.State == MEM_FREE && mbi.BaseAddress == region->end_reserved) {
        expected = mbi.RegionSize;
    }
    FL_ASSERT_EQ_SIZE_T(expected, available);

    // Restore
    if (uncommitted > 0) {
        decommit(region, uncommitted);
    }
}
