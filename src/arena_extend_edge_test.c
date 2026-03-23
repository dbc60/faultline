/**
 * @file arena_extend_edge_test.c
 * @author Douglas Cuthbertson
 * @brief Test cases for arena region extension (Group 5) and edge cases (Group 6).
 * @version 0.1
 * @date 2026-02-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Group 5 - Region Extension:
 *  24. extend_within_reserved
 *  25. extend_new_region_node
 *  26. extend_updates_footprint
 *
 * Group 6 - Edge Cases:
 *  27. allocate_zero_bytes
 *  28. allocate_one_byte
 *  29. allocate_boundary_small_large
 *  30. release_arena_cleans_region_nodes
 *  31. allocate_exact_top_size
 *  32. release_adjacent_to_exhausted_top
 */
// Test framework
#include <faultline/fl_test.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/fl_macros.h>

// ============================================================================
// Test fixture
// ============================================================================

typedef struct ArenaExtendEdgeTest {
    FLTestCase tc;
    Arena     *arena;
    Arena     *blocker; // second arena to block in-place region extension
} ArenaExtendEdgeTest;

static FL_SETUP_FN(setup_extend_reserved) {
    ArenaExtendEdgeTest *t = FL_CONTAINER_OF(tc, ArenaExtendEdgeTest, tc);
    t->arena               = new_arena(KIBI(4), 4);
    t->blocker             = NULL;
}

static FL_SETUP_FN(setup_extend_blocked) {
    ArenaExtendEdgeTest *t = FL_CONTAINER_OF(tc, ArenaExtendEdgeTest, tc);
    // Allocate two arenas back-to-back. The blocker arena is placed right after
    // the first in virtual address space, preventing the first arena's region
    // from extending in-place. This forces extend() to create a new region node.
    t->arena   = new_arena(KIBI(4), 0);
    t->blocker = new_arena(KIBI(4), 0);
}

static FL_SETUP_FN(setup_edge) {
    ArenaExtendEdgeTest *t = FL_CONTAINER_OF(tc, ArenaExtendEdgeTest, tc);
    t->arena               = new_arena(MEBI(1), 0);
    t->blocker             = NULL;
}

static FL_CLEANUP_FN(cleanup_extend_edge) {
    ArenaExtendEdgeTest *t = FL_CONTAINER_OF(tc, ArenaExtendEdgeTest, tc);
    if (t->arena != NULL) {
        release_arena(&t->arena);
    }
    if (t->blocker != NULL) {
        release_arena(&t->blocker);
    }
}

// ============================================================================
// Group 5: Region Extension
// ============================================================================

// ============================================================================
// Test 24: extend_within_reserved
// Create arena with extra reserved blocks. Exhaust committed space, then
// allocate. Verify extend succeeds without creating a new region node.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Extend Within Reserved", ArenaExtendEdgeTest,
                           extend_within_reserved, setup_extend_reserved,
                           cleanup_extend_edge) {
    Arena *arena = t->arena;

    size_t initial_footprint = arena_footprint(arena);
    size_t initial_available = region_available_bytes(MEM_TO_REGION(arena));

    // Region list should be empty initially
    FL_ASSERT_TRUE(DLIST_IS_EMPTY(&arena->region_list));

    // Exhaust committed space by allocating until footprint increases
    void  *ptrs[128];
    size_t count = 0;

    while (count < FL_ARRAY_COUNT(ptrs)) {
        ptrs[count] = arena_malloc_throw(arena, KIBI(1), __FILE__, __LINE__);
        count++;

        if (arena_footprint(arena) > initial_footprint) {
            break;
        }
    }

    // Extend happened within reserved space
    FL_ASSERT_GT_SIZE_T(arena_footprint(arena), initial_footprint);

    // Available reserved bytes should have decreased
    FL_ASSERT_LT_SIZE_T(region_available_bytes(MEM_TO_REGION(arena)), initial_available);

    // No new region node was needed - region list still empty
    FL_ASSERT_TRUE(DLIST_IS_EMPTY(&arena->region_list));
}

// ============================================================================
// Test 25: extend_new_region_node
// Create arena with no extra reserve. Exhaust committed space, then allocate.
// Verify a new region node is created (region list non-empty).
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Extend New Region Node", ArenaExtendEdgeTest,
                           extend_new_region_node, setup_extend_blocked,
                           cleanup_extend_edge) {
    Arena *arena = t->arena;

    // Region list should be empty initially
    FL_ASSERT_TRUE(DLIST_IS_EMPTY(&arena->region_list));

    // Exhaust committed + reserved space in the primary region, leaving only
    // CHUNK_MIN_SIZE. The blocker arena prevents in-place region extension.
    size_t top_size = CHUNK_SIZE(arena->top);
    size_t reserved = (size_t)(MEM_TO_REGION(arena)->end_reserved
                               - MEM_TO_REGION(arena)->end_committed);
    size_t request  = reserved + top_size - CHUNK_ALIGNED_SIZE - CHUNK_MIN_SIZE;

    arena_malloc_throw(arena, request, __FILE__, __LINE__);

    // Region list should still be empty (extended within reserved space)
    FL_ASSERT_TRUE(DLIST_IS_EMPTY(&arena->region_list));

    // Now allocate again - the blocker prevents in-place extension, so a new
    // region node must be created
    arena_malloc_throw(arena, KIBI(1), __FILE__, __LINE__);

    // A new region node should have been created
    FL_ASSERT_FALSE(DLIST_IS_EMPTY(&arena->region_list));
}

// ============================================================================
// Test 26: extend_updates_footprint
// After extend, verify footprint and max_footprint are updated.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Extend Updates Footprint", ArenaExtendEdgeTest,
                           extend_updates_footprint, setup_extend_reserved,
                           cleanup_extend_edge) {
    Arena *arena = t->arena;

    size_t initial_footprint     = arena_footprint(arena);
    size_t initial_max_footprint = arena_max_footprint(arena);

    // Exhaust committed space to trigger extend
    void  *ptrs[128];
    size_t count = 0;

    while (count < FL_ARRAY_COUNT(ptrs)) {
        ptrs[count] = arena_malloc_throw(arena, KIBI(1), __FILE__, __LINE__);
        count++;

        if (arena_footprint(arena) > initial_footprint) {
            break;
        }
    }

    // Footprint increased
    FL_ASSERT_GT_SIZE_T(arena_footprint(arena), initial_footprint);

    // Max footprint tracks the peak and is >= current footprint
    FL_ASSERT_GE_SIZE_T(arena_max_footprint(arena), arena_footprint(arena));

    // Max footprint is at least as large as the initial max
    FL_ASSERT_GE_SIZE_T(arena_max_footprint(arena), initial_max_footprint);
}

// ============================================================================
// Group 6: Edge Cases
// ============================================================================

// ============================================================================
// Test 27: allocate_zero_bytes
// Request 0 bytes. Verify a valid pointer is returned with at least
// CHUNK_MIN_PAYLOAD usable bytes.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Allocate Zero Bytes", ArenaExtendEdgeTest,
                           allocate_zero_bytes, setup_edge, cleanup_extend_edge) {
    Arena *arena = t->arena;

    void *mem = arena_malloc_throw(arena, 0, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(mem);

    // Should have a valid allocation
    FL_ASSERT_EQ_SIZE_T((size_t)1, arena_allocation_count(arena));

    // The chunk backing this allocation should be at least CHUNK_MIN_SIZE
    FL_ASSERT_GE_SIZE_T(arena_allocation_size(mem), CHUNK_MIN_SIZE);
}

// ============================================================================
// Test 28: allocate_one_byte
// Request 1 byte. Verify returned chunk is at least CHUNK_MIN_SIZE.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Allocate One Byte", ArenaExtendEdgeTest, allocate_one_byte,
                           setup_edge, cleanup_extend_edge) {
    Arena *arena = t->arena;

    void *mem = arena_malloc_throw(arena, 1, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(mem);

    // Chunk must be at least CHUNK_MIN_SIZE
    size_t alloc_size = arena_allocation_size(mem);
    FL_ASSERT_GE_SIZE_T(alloc_size, CHUNK_MIN_SIZE);

    // Chunk must be aligned
    FL_ASSERT_EQ_SIZE_T((size_t)0, alloc_size & CHUNK_ALIGNMENT_MASK);
}

// ============================================================================
// Test 29: allocate_boundary_small_large
// Allocate ARENA_MAX_SMALL_REQUEST and ARENA_MAX_SMALL_REQUEST+1.
// Verify the first produces a small chunk and the second a large chunk.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Allocate Boundary Small/Large", ArenaExtendEdgeTest,
                           allocate_boundary_small_large, setup_edge,
                           cleanup_extend_edge) {
    Arena *arena = t->arena;

    // Allocate at the small/large boundary
    void *small_mem
        = arena_malloc_throw(arena, ARENA_MAX_SMALL_REQUEST, __FILE__, __LINE__);
    size_t small_size = arena_allocation_size(small_mem);

    // Should be a small chunk
    FL_ASSERT_GE_SIZE_T(small_size, CHUNK_SIZE_FROM_REQUEST(ARENA_MAX_SMALL_REQUEST));
    FL_ASSERT_TRUE(ARENA_IS_SIZE_SMALL(small_size));

    // Allocate one byte beyond the small boundary
    void *large_mem
        = arena_malloc_throw(arena, ARENA_MAX_SMALL_REQUEST + 1, __FILE__, __LINE__);
    size_t large_size = arena_allocation_size(large_mem);

    // Should be a large chunk
    FL_ASSERT_GE_SIZE_T(large_size, (size_t)ARENA_MIN_LARGE_CHUNK);
}

// ============================================================================
// Test 30: release_arena_cleans_region_nodes
// Create arena, trigger region node creation, then release_arena. Verify
// pointer is set to NULL (confirms cleanup completed without crashing).
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release Arena Cleans Region Nodes", ArenaExtendEdgeTest,
                           release_arena_cleans_region_nodes, setup_extend_blocked,
                           cleanup_extend_edge) {
    Arena *arena = t->arena;

    // Exhaust committed + reserved space, leaving only CHUNK_MIN_SIZE.
    // The blocker arena prevents in-place extension.
    size_t top_size = CHUNK_SIZE(arena->top);
    size_t reserved = (size_t)(MEM_TO_REGION(arena)->end_reserved
                               - MEM_TO_REGION(arena)->end_committed);
    size_t request  = reserved + top_size - CHUNK_ALIGNED_SIZE - CHUNK_MIN_SIZE;

    arena_malloc_throw(arena, request, __FILE__, __LINE__);

    // Force a new region node (blocker prevents in-place extension)
    arena_malloc_throw(arena, KIBI(1), __FILE__, __LINE__);
    FL_ASSERT_FALSE(DLIST_IS_EMPTY(&arena->region_list));

    // Release arena - should clean up region nodes without crashing
    release_arena(&t->arena);

    // Pointer should be NULL after release
    FL_ASSERT_NULL(t->arena);
}

// ============================================================================
// Test 31: allocate_exact_top_size
// Allocate exactly the remaining top size. Verify top advances to sentinel.
// Next allocation should trigger extend.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Allocate Exact Top Size", ArenaExtendEdgeTest,
                           allocate_exact_top_size, setup_edge, cleanup_extend_edge) {
    Arena *arena = t->arena;

    // Get the current top size and compute a request that exactly consumes it
    size_t top_size = CHUNK_SIZE(arena->top);
    size_t request  = top_size - CHUNK_ALIGNED_SIZE;

    size_t footprint_before = arena_footprint(arena);

    arena_malloc_throw(arena, request, __FILE__, __LINE__);

    // Top should now point to the sentinel (size == CHUNK_SENTINEL_SIZE)
    FL_ASSERT_EQ_SIZE_T(CHUNK_SENTINEL_SIZE, CHUNK_SIZE(arena->top));

    // Next allocation should trigger extend, increasing the footprint
    arena_malloc_throw(arena, 1, __FILE__, __LINE__);
    FL_ASSERT_GT_SIZE_T(arena_footprint(arena), footprint_before);
}

// ============================================================================
// Test 32: release_adjacent_to_exhausted_top
//
// Regression test for the sentinel PREV_INUSE corruption bug.
//
// When allocate_from_top() exhausts the top with no remainder, arena->top is
// set to the sentinel, which has CHUNK_CURRENT_INUSE_FLAG set. A subsequent
// free of the chunk immediately preceding the sentinel called
// CHUNK_CLEAR_PREVIOUS_INUSE(sentinel) but then skipped the forward-merge
// block because !CHUNK_IS_INUSE(sentinel) was false. The cleared PREV_INUSE
// was never restored, so the next arena_malloc_throw fired:
//   FL_ASSERT(CHUNK_IS_PREVIOUS_INUSE(top))  in dbg_check_top_chunk.
//
// The fix moves the next == arena->top check outside the !CHUNK_IS_INUSE
// guard, so the freed chunk always becomes the new top regardless of whether
// the current top is a free chunk or the exhausted sentinel.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Free Adjacent to Exhausted Top",
                           ArenaExtendEdgeTest, release_adjacent_to_exhausted_top,
                           setup_edge, cleanup_extend_edge) {
    Arena *arena = t->arena;

    // Allocate a guard so the last chunk has an in-use predecessor, ensuring
    // previous_inuse == true when it is freed (no backward merge confusion).
    void *mem_guard = arena_malloc_throw(arena, 16, __FILE__, __LINE__);

    // Compute a request that exactly consumes the remaining top with no remainder.
    size_t top_size = CHUNK_SIZE(arena->top);
    size_t request  = top_size - CHUNK_ALIGNED_SIZE;

    void  *mem_last   = arena_malloc_throw(arena, request, __FILE__, __LINE__);
    Chunk *chunk_last = CHUNK_FROM_MEMORY(mem_last);

    // Top should now be the sentinel (exhausted): size == CHUNK_SENTINEL_SIZE
    // and CHUNK_CURRENT_INUSE_FLAG is set.
    FL_ASSERT_EQ_SIZE_T(CHUNK_SENTINEL_SIZE, CHUNK_SIZE(arena->top));

    // Free the chunk adjacent to the sentinel. Before the fix, this cleared
    // sentinel->PREV_INUSE and left arena->top corrupted.
    arena_free_throw(arena, mem_last, __FILE__, __LINE__);

    // The freed chunk must become the new top.
    FL_ASSERT_EQ_PTR((void *)chunk_last, (void *)arena->top);

    // The new top must be free.
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(arena->top));

    // Key invariant: top's PREV_INUSE must be set (the guard chunk before it
    // is still in use). Before the fix this was 0, triggering the assertion in
    // dbg_check_top_chunk on the very next malloc call.
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(arena->top));

    // Allocate again. Without the fix, dbg_check_top_chunk fired here.
    void *mem_probe = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(mem_probe);

    // Cleanup
    arena_free_throw(arena, mem_probe, __FILE__, __LINE__);
    arena_free_throw(arena, mem_guard, __FILE__, __LINE__);
}
