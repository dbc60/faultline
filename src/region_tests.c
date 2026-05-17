/**
 * @file region_tests.c
 * @author Douglas Cuthbertson
 * @brief A test suite for the Region library.
 * @version 0.2
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include "region.c"
#include "region_windows_test.c"

#include "region_test.h"                           // RegionAllocationTestCase typedef
#include <faultline/fl_exception_service_assert.h> // assert macro
#include <faultline/fl_macros.h>                   // FL_SPEC_EXPORT
#include <faultline/fl_test.h>                     // FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h> // FL_TRY, FL_CATCH, FL_THROW_DETAILS_FILE_LINE
#include <stddef.h>           // size_t

/**
 * @brief Allocate a new Region with default commit and reserve (0, 0).
 */
static void setup_region_allocation(FLTestCase *data) {
    RegionAllocationTestCase *tc = FL_CONTAINER_OF(data, RegionAllocationTestCase, tc);
    tc->region                   = new_region(0, 0);
    FL_ASSERT_GE_SIZE_T(REGION_BYTES_RESERVED(tc->region), REGION_ALIGNED_SIZE);
}

static void cleanup_region_allocation(FLTestCase *data) {
    RegionAllocationTestCase *tc = FL_CONTAINER_OF(data, RegionAllocationTestCase, tc);
    if (tc->region != NULL) {
        release_region(tc->region);
        tc->region = NULL;
    }
}

/**
 * @brief verify that a newly allocated Region's fields are correctly
 * initialized.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Allocation", RegionAllocationTestCase,
                           test_region_allocation, setup_region_allocation,
                           cleanup_region_allocation) {
    Region *region = t->region;
    char   *top    = (char *)region;
    u32     granularity, page_size;

    get_memory_info(&granularity, &page_size);

    size_t committed = ALIGN_UP(sizeof(Region), page_size);
    size_t reserved  = ALIGN_UP(sizeof(Region), granularity);

    FL_ASSERT_EQ_PTR(top + reserved, (void *)region->end_reserved);
    FL_ASSERT_EQ_PTR(top + committed, (void *)region->end_committed);
    FL_ASSERT_EQ_SIZE_T(reserved, REGION_BYTES_RESERVED(region));
    FL_ASSERT_EQ_UINT32(granularity, region->granularity);
    FL_ASSERT_EQ_UINT32(page_size, region->page_size);
    FL_ASSERT_EQ_SIZE_T(committed, REGION_BYTES_COMMITTED(region));
}

/**
 * @brief verify commit and decommit work as expected
 */
FL_TYPE_TEST_SETUP_CLEANUP("Commit/Decommit", RegionAllocationTestCase,
                           test_commit_decommit, setup_region_allocation,
                           cleanup_region_allocation) {
    Region      *region      = t->region;
    char        *initial_top = region->end_committed;
    size_t const page_size   = region->page_size;

    // commit/decommit memory. The results should always be a multiple of a page size.
    for (size_t i = page_size; i < region->granularity; i += page_size) {
        FL_TRY {
            commit(region, i);
        }
        FL_CATCH(region_out_of_memory) {
            FL_THROW_DETAILS_FILE_LINE(FL_REASON, "page size (%zu): %s", FL_FILE,
                                       FL_LINE, i, FL_DETAILS);
        }
        FL_END_TRY;

        FL_ASSERT_EQ_SIZE_T(i, (size_t)(region->end_committed - initial_top));

        decommit(region, i);
        FL_ASSERT_EQ_PTR((void *)initial_top, (void *)region->end_committed);
    }
}

// Must ensure the allocated region has enough granularity-sized blocks just after it so
// expand won't throw region_out_of_memory.
static void setup_expand_shrink(FLTestCase *tc) {
    RegionAllocationTestCase *ratc = FL_CONTAINER_OF(tc, RegionAllocationTestCase, tc);
    ratc->region                   = new_region(0, 16);
    FL_ASSERT_GE_SIZE_T(REGION_BYTES_RESERVED(ratc->region), sizeof(Region));
}

FL_TYPE_TEST_SETUP_CLEANUP("Expand/Shrink", RegionAllocationTestCase, test_expand_shrink,
                           setup_expand_shrink, cleanup_region_allocation) {
    Region     *region         = t->region;
    size_t      request        = 16 * region->granularity;
    size_t      to_commit      = REGION_TO_COMMIT(region, request);
    size_t      bytes_reserved = REGION_BYTES_RESERVED(region);
    char const *committed      = region->end_committed;
    size_t      commit_size    = ALIGN_UP(to_commit, region->page_size);

    FL_ASSERT_TRUE(REGION_IS_RESERVED(region));
    FL_ASSERT_GT_SIZE_T(to_commit, (size_t)0);
    FL_ASSERT_LE_SIZE_T(to_commit, request);

    size_t available = region_available_bytes(t->region);
    if (available < commit_size) {
        FL_FAIL("not enough reserved space: available (%zu) < commit_size (%zu)",
                available, commit_size);
    }

    region_extend(region, commit_size);
    FL_ASSERT_EQ_SIZE_T((size_t)(16 * region->granularity - region->page_size),
                        (size_t)(region->end_committed - committed));

    shrink(region, request, commit_size);
    FL_ASSERT_EQ_PTR((void *)committed, (void *)region->end_committed);
    FL_ASSERT_EQ_SIZE_T(bytes_reserved, REGION_BYTES_RESERVED(region));
}

// ---------------------------------------------------------------------------
// new_custom_region with multipliers
// ---------------------------------------------------------------------------

static void setup_custom_2x(FLTestCase *data) {
    RegionAllocationTestCase *tc = FL_CONTAINER_OF(data, RegionAllocationTestCase, tc);
    tc->region                   = new_custom_region(0, 0, 2, 2);
}

FL_TYPE_TEST_SETUP_CLEANUP("Custom Multipliers", RegionAllocationTestCase,
                           test_custom_multipliers, setup_custom_2x,
                           cleanup_region_allocation) {
    Region *region = t->region;
    u32     granularity, page_size;

    get_memory_info(&granularity, &page_size);

    // Applied granularity should be 2x system granularity
    FL_ASSERT_EQ_UINT32(granularity * 2, region->granularity);

    // Applied page size should be 2x system page size
    FL_ASSERT_EQ_UINT32(page_size * 2, region->page_size);

    // Reserved should be aligned to applied granularity
    FL_ASSERT_TRUE(REGION_BYTES_RESERVED(region) % region->granularity == 0);

    // Committed should be aligned to applied page size
    FL_ASSERT_TRUE(REGION_BYTES_COMMITTED(region) % region->page_size == 0);
}

// ---------------------------------------------------------------------------
// new_custom_region error paths
// ---------------------------------------------------------------------------

FL_TEST("Granularity Overflow", test_granularity_overflow) {
    u32 granularity, page_size;
    get_memory_info(&granularity, &page_size);

    // Multiplier that causes granularity * mult to wrap u32 to 0
    u32 overflow_mult = (u32)(0x100000000ULL / granularity);

    FL_TRY {
        Region *r = new_custom_region(0, 0, overflow_mult, 0);
        release_region(r);
        FL_FAIL("Expected region_initialization_failure for granularity overflow");
    }
    FL_CATCH(region_initialization_failure) {
        // expected
    }
    FL_END_TRY;
}

FL_TEST("Page Size Overflow", test_page_size_overflow) {
    u32 granularity, page_size;
    get_memory_info(&granularity, &page_size);

    // Multiplier that causes page_size * mult to wrap u32 to 0
    u32 overflow_mult = (u32)(0x100000000ULL / page_size);

    FL_TRY {
        Region *r = new_custom_region(0, 0, 0, overflow_mult);
        release_region(r);
        FL_FAIL("Expected region_initialization_failure for page size overflow");
    }
    FL_CATCH(region_initialization_failure) {
        // expected
    }
    FL_END_TRY;
}

FL_TEST("Page Size > Granularity", test_page_gt_granularity) {
    u32 granularity, page_size;
    get_memory_info(&granularity, &page_size);

    // Make applied page size exceed applied granularity
    u32 ps_mult = (granularity / page_size) + 1;

    FL_TRY {
        Region *r = new_custom_region(0, 0, 0, ps_mult);
        release_region(r);
        FL_FAIL("Expected region_initialization_failure for page size > granularity");
    }
    FL_CATCH(region_initialization_failure) {
        // expected
    }
    FL_END_TRY;
}

// ---------------------------------------------------------------------------
// REGION_ALIGNED_SIZE
// ---------------------------------------------------------------------------

FL_TEST("REGION_ALIGNED_SIZE", test_region_aligned_size) {
    // Must be a multiple of REGION_ALIGNMENT
    FL_ASSERT_TRUE(REGION_ALIGNED_SIZE % REGION_ALIGNMENT == 0);

    // Must be at least sizeof(Region)
    FL_ASSERT_TRUE(REGION_ALIGNED_SIZE >= sizeof(Region));
}

// ---------------------------------------------------------------------------
// REGION_TO_MEM / MEM_TO_REGION round-trip
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("REGION_TO_MEM / MEM_TO_REGION", RegionAllocationTestCase,
                           test_region_mem_macros, setup_region_allocation,
                           cleanup_region_allocation) {
    Region *region = t->region;

    void   *mem  = REGION_TO_MEM(region);
    Region *back = MEM_TO_REGION(mem);

    // Round-trip should return the original pointer
    FL_ASSERT_EQ_PTR(region, back);

    // mem should be past the region header
    FL_ASSERT_TRUE((char *)mem > (char *)region);

    // mem should be at the aligned offset
    FL_ASSERT_EQ_PTR((char *)region + REGION_ALIGNED_SIZE, mem);
}

// ---------------------------------------------------------------------------
// Commit preserves reserved
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("Commit Preserves Reserved", RegionAllocationTestCase,
                           test_commit_preserves_reserved, setup_region_allocation,
                           cleanup_region_allocation) {
    Region *region          = t->region;
    size_t  reserved_before = REGION_BYTES_RESERVED(region);
    size_t  page_size       = region->page_size;

    // Commit one page
    commit(region, page_size);
    FL_ASSERT_EQ_SIZE_T(reserved_before, REGION_BYTES_RESERVED(region));

    // Decommit it
    decommit(region, page_size);
    FL_ASSERT_EQ_SIZE_T(reserved_before, REGION_BYTES_RESERVED(region));
}

// ---------------------------------------------------------------------------
// Non-zero initial commit
// ---------------------------------------------------------------------------

static void setup_nonzero_commit(FLTestCase *data) {
    RegionAllocationTestCase *tc = FL_CONTAINER_OF(data, RegionAllocationTestCase, tc);
    tc->region                   = new_region(KIBI(64), 0);
}

FL_TYPE_TEST_SETUP_CLEANUP("Non-zero Initial Commit", RegionAllocationTestCase,
                           test_nonzero_commit, setup_nonzero_commit,
                           cleanup_region_allocation) {
    Region *region = t->region;
    u32     page_size;

    get_memory_info(0, &page_size);

    // Committed should be at least sizeof(Region) + 64KiB, rounded to page size
    size_t expected_min = ALIGN_UP(REGION_ALIGNED_SIZE + KIBI(64), page_size);
    FL_ASSERT_TRUE(REGION_BYTES_COMMITTED(region) >= expected_min);

    // Committed should be page-aligned
    FL_ASSERT_TRUE(REGION_BYTES_COMMITTED(region) % page_size == 0);

    // Reserved should still be granularity-aligned
    FL_ASSERT_TRUE(REGION_BYTES_RESERVED(region) % region->granularity == 0);
}

// ---------------------------------------------------------------------------
// Multiple sequential extends
// ---------------------------------------------------------------------------

static void setup_many_blocks(FLTestCase *data) {
    RegionAllocationTestCase *tc = FL_CONTAINER_OF(data, RegionAllocationTestCase, tc);
    tc->region                   = new_region(0, 32);
}

FL_TYPE_TEST_SETUP_CLEANUP("Multiple Extends", RegionAllocationTestCase,
                           test_multiple_extends, setup_many_blocks,
                           cleanup_region_allocation) {
    Region *region           = t->region;
    size_t  page_size        = region->page_size;
    char   *committed_before = region->end_committed;

    // Extend three times by one page each
    for (int i = 0; i < 3; i++) {
        size_t committed = region_extend(region, page_size);
        if (committed < page_size) {
            FL_FAIL("extend %d: committed %zu < page_size %zu", i, committed, page_size);
        }
    }

    // Total committed should have grown by at least 3 pages
    size_t total_growth = (size_t)(region->end_committed - committed_before);
    FL_ASSERT_TRUE(total_growth >= 3 * page_size);

    // Committed should still be page-aligned
    FL_ASSERT_TRUE(REGION_BYTES_COMMITTED(region) % page_size == 0);
}

// ---------------------------------------------------------------------------
// Extend with zero request
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("Extend Zero", RegionAllocationTestCase, test_extend_zero,
                           setup_region_allocation, cleanup_region_allocation) {
    Region *region           = t->region;
    char   *committed_before = region->end_committed;

    size_t committed = region_extend(region, 0);

    FL_ASSERT_EQ_SIZE_T((size_t)0, committed);
    FL_ASSERT_EQ_PTR((void *)committed_before, (void *)region->end_committed);
}

// ---------------------------------------------------------------------------
// Extend beyond available space
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("Extend Beyond Available", RegionAllocationTestCase,
                           test_extend_beyond_available, setup_region_allocation,
                           cleanup_region_allocation) {
    // Request far more than any system can provide (1 PiB)
    FL_TRY {
        region_extend(t->region, TEBI(1024));
        FL_FAIL("Expected region_out_of_memory for extend beyond available space");
    }
    FL_CATCH(region_out_of_memory) {
        // expected
    }
    FL_END_TRY;
}

// ---------------------------------------------------------------------------
// new_custom_region with multiplier=1 behaves like multiplier=0
// ---------------------------------------------------------------------------

FL_TEST("Custom Multiplier One", test_custom_multiplier_one) {
    Region *r0 = new_custom_region(0, 0, 0, 0);
    Region *r1 = new_custom_region(0, 0, 1, 1);

    FL_ASSERT_EQ_UINT32(r0->granularity, r1->granularity);
    FL_ASSERT_EQ_UINT32(r0->page_size, r1->page_size);
    FL_ASSERT_EQ_SIZE_T(REGION_BYTES_RESERVED(r0), REGION_BYTES_RESERVED(r1));
    FL_ASSERT_EQ_SIZE_T(REGION_BYTES_COMMITTED(r0), REGION_BYTES_COMMITTED(r1));

    release_region(r1);
    release_region(r0);
}

// ---------------------------------------------------------------------------
// Committed alignment after extend with non-aligned request
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("Committed Alignment After Extend", RegionAllocationTestCase,
                           test_committed_alignment, setup_many_blocks,
                           cleanup_region_allocation) {
    Region *region    = t->region;
    size_t  page_size = region->page_size;

    // Extend by a non-page-aligned amount to verify rounding
    region_extend(region, page_size + 1);
    FL_ASSERT_TRUE(REGION_BYTES_COMMITTED(region) % page_size == 0);

    // Extend again by another non-aligned amount
    region_extend(region, page_size * 3 + 7);
    FL_ASSERT_TRUE(REGION_BYTES_COMMITTED(region) % page_size == 0);
}

// ---------------------------------------------------------------------------
// REGION_TO_MEM alignment
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("REGION_TO_MEM Alignment", RegionAllocationTestCase,
                           test_region_to_mem_alignment, setup_region_allocation,
                           cleanup_region_allocation) {
    void *mem = REGION_TO_MEM(t->region);

    // The returned pointer must be aligned to REGION_ALIGNMENT
    FL_ASSERT_TRUE((size_t)mem % REGION_ALIGNMENT == 0);
}

// ---------------------------------------------------------------------------
// new_region with large commit (more than one granularity)
// ---------------------------------------------------------------------------

FL_TEST("Large Initial Commit", test_large_initial_commit) {
    u32 granularity;
    get_memory_info(&granularity, 0);

    // Request more than one granularity worth of committed space
    size_t  request = (size_t)granularity * 2;
    Region *region  = new_region(request, 0);

    // Committed should be at least request + REGION_ALIGNED_SIZE, page-aligned
    FL_ASSERT_GE_SIZE_T(REGION_BYTES_COMMITTED(region),
                        ALIGN_UP(request + REGION_ALIGNED_SIZE, region->page_size));

    // Reserved should be at least committed
    FL_ASSERT_GE_SIZE_T(REGION_BYTES_RESERVED(region), REGION_BYTES_COMMITTED(region));

    release_region(region);
}

// ---------------------------------------------------------------------------
// release_region then re-allocate
// ---------------------------------------------------------------------------

FL_TEST("Release Then Re-allocate", test_release_reallocate) {
    Region *region      = new_region(0, 0);
    u32     granularity = region->granularity;
    u32     page_size   = region->page_size;

    release_region(region);

    // Allocate a new region - should succeed
    Region *region2 = new_region(0, 0);
    FL_ASSERT_TRUE(region2 != NULL);
    FL_ASSERT_EQ_UINT32(granularity, region2->granularity);
    FL_ASSERT_EQ_UINT32(page_size, region2->page_size);

    release_region(region2);
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(test_region_allocation)
FL_SUITE_ADD_EMBEDDED(test_reserve)
FL_SUITE_ADD_EMBEDDED(test_absurd_reservation)
FL_SUITE_ADD_EMBEDDED(test_commit_decommit)
FL_SUITE_ADD_EMBEDDED(test_expand_shrink)
FL_SUITE_ADD_EMBEDDED(test_custom_multipliers)
FL_SUITE_ADD_EMBEDDED(test_region_mem_macros)
FL_SUITE_ADD_EMBEDDED(test_commit_preserves_reserved)
FL_SUITE_ADD_EMBEDDED(test_nonzero_commit)
FL_SUITE_ADD_EMBEDDED(test_multiple_extends)
FL_SUITE_ADD_EMBEDDED(test_available_bytes)
FL_SUITE_ADD_EMBEDDED(test_can_extend)
FL_SUITE_ADD_EMBEDDED(test_extend_zero)
FL_SUITE_ADD_EMBEDDED(test_extend_beyond_available)
FL_SUITE_ADD_EMBEDDED(test_committed_alignment)
FL_SUITE_ADD_EMBEDDED(test_region_to_mem_alignment)
FL_SUITE_ADD_EMBEDDED(test_available_after_full_commit)
FL_SUITE_ADD(test_granularity_overflow)
FL_SUITE_ADD(test_page_size_overflow)
FL_SUITE_ADD(test_page_gt_granularity)
FL_SUITE_ADD(test_region_aligned_size)
FL_SUITE_ADD(test_custom_multiplier_one)
FL_SUITE_ADD(test_large_initial_commit)
FL_SUITE_ADD(test_release_reallocate)
FL_SUITE_END;

FL_GET_TEST_SUITE("Region", ts)
