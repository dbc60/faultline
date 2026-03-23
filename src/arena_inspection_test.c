/**
 * @file arena_inspection_test.c
 * @author Douglas Cuthbertson
 * @brief Test cases for mid-level arena inspection APIs (Group 3).
 * @version 0.1
 * @date 2026-02-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Tests the following inspection APIs:
 *  15. footprint_after_init
 *  16. footprint_after_extend
 *  17. max_footprint_tracks_peak
 *  18. allocation_count_lifecycle
 *  19. allocation_size_returns_chunk_size
 *  20. is_valid_allocation_true
 *  21. is_valid_allocation_false
 */
// Test framework
#include <faultline/fl_test.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/fl_macros.h>

#include <stdint.h> // uintptr_t

// ============================================================================
// Test fixture
// ============================================================================

typedef struct ArenaInspectionTest {
    FLTestCase tc;
    Arena     *arena;
} ArenaInspectionTest;

static FL_SETUP_FN(setup_inspection) {
    ArenaInspectionTest *t = FL_CONTAINER_OF(tc, ArenaInspectionTest, tc);
    t->arena               = new_arena(MEBI(1), 0);
}

static FL_SETUP_FN(setup_inspection_small) {
    ArenaInspectionTest *t = FL_CONTAINER_OF(tc, ArenaInspectionTest, tc);
    t->arena               = new_arena(KIBI(4), 0);
}

static FL_CLEANUP_FN(cleanup_inspection) {
    ArenaInspectionTest *t = FL_CONTAINER_OF(tc, ArenaInspectionTest, tc);
    release_arena(&t->arena);
}

// ============================================================================
// Test 15: footprint_after_init
// Create arena, verify arena_footprint equals committed region size.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Footprint After Init", ArenaInspectionTest,
                           footprint_after_init, setup_inspection, cleanup_inspection) {
    Arena *arena = t->arena;

    size_t footprint = arena_footprint(arena);

    // Footprint should be nonzero after initialization
    FL_ASSERT_GT_SIZE_T(footprint, (size_t)0);

    // Footprint should match the committed bytes of the primary region
    size_t committed = REGION_BYTES_COMMITTED(MEM_TO_REGION(arena));
    FL_ASSERT_EQ_SIZE_T(committed, footprint);
}

// ============================================================================
// Test 16: footprint_after_extend
// Exhaust top, allocate to trigger extend. Verify footprint increased.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Footprint After Extend", ArenaInspectionTest,
                           footprint_after_extend, setup_inspection_small,
                           cleanup_inspection) {
    Arena *arena             = t->arena;
    size_t initial_footprint = arena_footprint(arena);

    // Allocate in a loop until the footprint changes (which means extend happened)
    size_t chunk_request = KIBI(1);
    void  *ptrs[64];
    size_t count = 0;

    while (count < FL_ARRAY_COUNT(ptrs)) {
        ptrs[count] = arena_malloc_throw(arena, chunk_request, __FILE__, __LINE__);
        count++;

        if (arena_footprint(arena) > initial_footprint) {
            break;
        }
    }

    FL_ASSERT_GT_SIZE_T(arena_footprint(arena), initial_footprint);
}

// ============================================================================
// Test 17: max_footprint_tracks_peak
// Allocate, extend, verify arena_max_footprint >= arena_footprint.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Max Footprint Tracks Peak", ArenaInspectionTest,
                           max_footprint_tracks_peak, setup_inspection,
                           cleanup_inspection) {
    Arena *arena = t->arena;

    size_t initial_max = arena_max_footprint(arena);
    size_t initial_fp  = arena_footprint(arena);

    // Initially, max_footprint should equal footprint
    FL_ASSERT_EQ_SIZE_T(initial_fp, initial_max);

    // After some allocations (that don't trigger extend), max should still match
    void *mem = arena_malloc_throw(arena, 256, __FILE__, __LINE__);
    FL_ASSERT_GE_SIZE_T(arena_max_footprint(arena), arena_footprint(arena));

    // max_footprint should never decrease, even after releasing
    arena_free_throw(arena, mem, __FILE__, __LINE__);
    FL_ASSERT_GE_SIZE_T(arena_max_footprint(arena), initial_max);
}

// ============================================================================
// Test 18: allocation_count_lifecycle
// Allocate N items, verify count == N. Free M, verify N - M. Free rest, verify 0.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Allocation Count Lifecycle", ArenaInspectionTest,
                           allocation_count_lifecycle, setup_inspection,
                           cleanup_inspection) {
    Arena *arena = t->arena;

    // Initially, no allocations
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena_allocation_count(arena));

    // Allocate 5 items
    enum {
        N = 5
    };

    void *ptrs[N];
    for (int i = 0; i < N; i++) {
        ptrs[i] = arena_malloc_throw(arena, 32, __FILE__, __LINE__);
    }
    FL_ASSERT_EQ_SIZE_T((size_t)N, arena_allocation_count(arena));

    // Free 3 items
    enum {
        M = 3
    };

    for (int i = 0; i < M; i++) {
        arena_free_throw(arena, ptrs[i], __FILE__, __LINE__);
    }
    FL_ASSERT_EQ_SIZE_T((size_t)(N - M), arena_allocation_count(arena));

    // Free the remaining 2
    for (int i = M; i < N; i++) {
        arena_free_throw(arena, ptrs[i], __FILE__, __LINE__);
    }
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena_allocation_count(arena));
}

// ============================================================================
// Test 19: allocation_size_returns_chunk_size
// Allocate various sizes, verify arena_allocation_size returns a value >= request.
// Verify it returns 0 for NULL.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Allocation Size Returns Chunk Size", ArenaInspectionTest,
                           allocation_size_returns_chunk_size, setup_inspection,
                           cleanup_inspection) {
    Arena *arena = t->arena;

    // NULL should return 0
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena_allocation_size(NULL));

    // Test a range of request sizes
    size_t requests[] = {1, 8, 16, 32, 64, 128, 256, 512, 1024, 4096};
    for (size_t i = 0; i < FL_ARRAY_COUNT(requests); i++) {
        size_t request = requests[i];
        void  *mem     = arena_malloc_throw(arena, request, __FILE__, __LINE__);

        size_t alloc_size = arena_allocation_size(mem);

        // The chunk size should be >= the request (it includes chunk overhead
        // and may be rounded up for alignment)
        FL_ASSERT_GE_SIZE_T(alloc_size, request);

        // The chunk size should be aligned
        FL_ASSERT_EQ_SIZE_T((size_t)0, alloc_size & CHUNK_ALIGNMENT_MASK);

        // The chunk size should be at least CHUNK_MIN_SIZE
        FL_ASSERT_GE_SIZE_T(alloc_size, CHUNK_MIN_SIZE);

        arena_free_throw(arena, mem, __FILE__, __LINE__);
    }
}

// ============================================================================
// Test 20: is_valid_allocation_true
// Allocate memory, verify arena_is_valid_allocation returns true.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Is Valid Allocation True", ArenaInspectionTest,
                           is_valid_allocation_true, setup_inspection,
                           cleanup_inspection) {
    Arena *arena = t->arena;

    void *mem = arena_malloc_throw(arena, 64, __FILE__, __LINE__);
    FL_ASSERT_TRUE(arena_is_valid_allocation(arena, mem));

    // Also valid for multiple allocations
    void *mem2 = arena_malloc_throw(arena, 256, __FILE__, __LINE__);
    FL_ASSERT_TRUE(arena_is_valid_allocation(arena, mem2));
    FL_ASSERT_TRUE(arena_is_valid_allocation(arena, mem)); // first still valid
}

// ============================================================================
// Test 21: is_valid_allocation_false
// Verify arena_is_valid_allocation returns false for a stack pointer and
// an address outside the arena.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Is Valid Allocation False", ArenaInspectionTest,
                           is_valid_allocation_false, setup_inspection,
                           cleanup_inspection) {
    Arena *arena = t->arena;

    // A stack variable should not be a valid allocation
    int stack_var = 42;
    FL_ASSERT_FALSE(arena_is_valid_allocation(arena, &stack_var));

    // NULL should not be a valid allocation
    FL_ASSERT_FALSE(arena_is_valid_allocation(arena, NULL));

    // An address far outside the arena should not be valid
    void *bogus = (void *)(uintptr_t)0xDEADBEEF;
    FL_ASSERT_FALSE(arena_is_valid_allocation(arena, bogus));
}
