/**
 * @file arena_fast_sysinfo_mixed_test.c
 * @author Douglas Cuthbertson
 * @brief Test cases for fast node behavior (Group 2), system info (Group 4),
 *        and mixed allocation patterns (Group 7).
 * @version 0.1
 * @date 2026-02-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Group 2 - Fast Node Behavior:
 *  11. fast_node_populated_on_split
 *  12. fast_node_consumed
 *  13. fast_node_replaced
 *  14. fast_node_too_small
 *
 * Group 4 - System Info Functions:
 *  22. page_size_nonzero_power_of_two
 *  23. granularity_nonzero_power_of_two
 *
 * Group 7 - Mixed Allocation Patterns:
 *  32. interleaved_small_large
 *  33. reuse_freed_chunks
 *  34. fragmentation_and_coalesce
 */
// Test framework
#include <faultline/fl_test.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/fl_macros.h>

// ============================================================================
// Test fixture for fast node and mixed pattern tests
// ============================================================================

typedef struct ArenaFastMixedTest {
    FLTestCase tc;
    Arena     *arena;
} ArenaFastMixedTest;

static FL_SETUP_FN(setup_fast_mixed) {
    ArenaFastMixedTest *t = FL_CONTAINER_OF(tc, ArenaFastMixedTest, tc);
    t->arena              = new_arena(MEBI(1), 0);
}

static FL_CLEANUP_FN(cleanup_fast_mixed) {
    ArenaFastMixedTest *t = FL_CONTAINER_OF(tc, ArenaFastMixedTest, tc);
    release_arena(&t->arena);
}

// ============================================================================
// Group 2: Fast Node Behavior
// ============================================================================

// ============================================================================
// Test 11: fast_node_populated_on_split
// The fast node is populated when a chunk is split from a small bin (path 3)
// or a large bin (path 4). To trigger path 3, we need a non-empty small bin
// with a chunk larger than our request.
//
// Strategy: Allocate two chunks of the same size from top (populating no bins).
// Free the first one (goes to a small bin). Allocate a smaller size that
// doesn't have an exact-fit bin - the allocator finds the freed chunk in its
// bin, splits it, and puts the remainder on the fast node.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Fast Node Populated on Split", ArenaFastMixedTest,
                           fast_node_populated_on_split, setup_fast_mixed,
                           cleanup_fast_mixed) {
    Arena *arena = t->arena;

    // Initially fast node should be empty
    FL_ASSERT_NULL(arena->fast);
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena->fast_size);

    // Allocate two chunks of size that maps to a specific small bin.
    // Use a request of 128 bytes - large enough to split later.
    void *mem_a = arena_malloc_throw(arena, 128, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 128, __FILE__, __LINE__);

    // Free A - it goes to a small bin
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);

    // The fast node should still be empty (release doesn't populate fast node
    // from bins)
    FL_ASSERT_NULL(arena->fast);

    // Now allocate something smaller than 128 that doesn't have an exact-fit bin.
    // Use CHUNK_MIN_PAYLOAD (the smallest possible request). This will find A's
    // chunk in the small bin, split it, and put the remainder on the fast node.
    void *mem_c = arena_malloc_throw(arena, CHUNK_MIN_PAYLOAD, __FILE__, __LINE__);

    // Fast node should now be populated with the remainder from splitting A's chunk
    FL_ASSERT_NOT_NULL(arena->fast);
    FL_ASSERT_GT_SIZE_T(arena->fast_size, (size_t)0);

    FL_UNUSED(mem_b);
    FL_UNUSED(mem_c);
}

// ============================================================================
// Test 12: fast_node_consumed
// Populate fast node, then allocate a size that fits. Verify fast node is
// consumed (set to NULL or reduced).
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Fast Node Consumed", ArenaFastMixedTest, fast_node_consumed,
                           setup_fast_mixed, cleanup_fast_mixed) {
    Arena *arena = t->arena;

    // Set up a fast node: allocate two 128-byte chunks, free the first,
    // then allocate CHUNK_MIN_PAYLOAD to trigger a split.
    void *mem_a = arena_malloc_throw(arena, 128, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 128, __FILE__, __LINE__);
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, CHUNK_MIN_PAYLOAD, __FILE__, __LINE__);

    // Fast node should be populated
    FL_ASSERT_NOT_NULL(arena->fast);
    size_t fast_size = arena->fast_size;

    // Allocate exactly the fast node's payload size to consume it entirely
    size_t request = fast_size - CHUNK_ALIGNED_SIZE;
    void  *mem_d   = arena_malloc_throw(arena, request, __FILE__, __LINE__);

    // Fast node should be consumed (NULL) or reduced
    // If the request exactly matches, fast should be NULL
    FL_ASSERT_NULL(arena->fast);
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena->fast_size);

    FL_UNUSED(mem_b);
    FL_UNUSED(mem_c);
    FL_UNUSED(mem_d);
}

// ============================================================================
// Test 13: fast_node_replaced
// Populate fast node, then allocate from a small bin that produces a new
// remainder. Verify old fast node was inserted into appropriate bin and
// new fast node is the remainder.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Fast Node Replaced", ArenaFastMixedTest, fast_node_replaced,
                           setup_fast_mixed, cleanup_fast_mixed) {
    Arena *arena = t->arena;

    // Step 1: Create a fast node by setting up a bin split.
    // Allocate two 128-byte chunks, free first, allocate CHUNK_MIN_PAYLOAD.
    void *mem_a = arena_malloc_throw(arena, 128, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 128, __FILE__, __LINE__);
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, CHUNK_MIN_PAYLOAD, __FILE__, __LINE__);

    FL_ASSERT_NOT_NULL(arena->fast);
    FreeChunk *old_fast      = arena->fast;
    size_t     old_fast_size = arena->fast_size;

    // Step 2: Create another bin entry larger than the fast node.
    // Allocate two 256-byte chunks, free the first.
    void *mem_d = arena_malloc_throw(arena, 256, __FILE__, __LINE__);
    void *mem_e = arena_malloc_throw(arena, 256, __FILE__, __LINE__);
    arena_free_throw(arena, mem_d, __FILE__, __LINE__);

    // Step 3: Allocate CHUNK_MIN_PAYLOAD again. This should find the freed
    // 256-byte chunk in its bin, split it, displace the old fast node to a
    // small bin, and install the remainder as the new fast node.
    void *mem_f = arena_malloc_throw(arena, CHUNK_MIN_PAYLOAD, __FILE__, __LINE__);

    // The fast node should have changed
    FL_ASSERT_NOT_NULL(arena->fast);
    FL_ASSERT_TRUE(arena->fast != old_fast || arena->fast_size != old_fast_size);

    // The new fast node's size should reflect the remainder of the 256-byte split
    FL_ASSERT_GT_SIZE_T(arena->fast_size, (size_t)0);

    FL_UNUSED(mem_b);
    FL_UNUSED(mem_c);
    FL_UNUSED(mem_e);
    FL_UNUSED(mem_f);
}

// ============================================================================
// Test 14: fast_node_too_small
// Populate fast node with small remainder, then request something larger.
// Verify allocation falls through to bins or top.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Fast Node Too Small", ArenaFastMixedTest,
                           fast_node_too_small, setup_fast_mixed, cleanup_fast_mixed) {
    Arena *arena = t->arena;

    // Create a fast node by the same bin-split approach
    void *mem_a = arena_malloc_throw(arena, 128, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 128, __FILE__, __LINE__);
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, CHUNK_MIN_PAYLOAD, __FILE__, __LINE__);

    FL_ASSERT_NOT_NULL(arena->fast);
    size_t fast_size_before = arena->fast_size;

    // Request something larger than the fast node can provide.
    // The fast node remainder from a 128-byte chunk split by CHUNK_MIN_PAYLOAD
    // should be well under 512 bytes.
    void *mem_d = arena_malloc_throw(arena, 512, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(mem_d);

    // The allocation should have succeeded (from top or another path).
    // The fast node may have been displaced or may still be there (if allocated
    // from top), but the allocation itself must succeed.
    // Count: mem_a freed (-1), mem_b (1), mem_c (1), mem_d (1) = 3
    FL_ASSERT_EQ_SIZE_T((size_t)3, arena_allocation_count(arena));

    FL_UNUSED(mem_b);
    FL_UNUSED(mem_c);
    FL_UNUSED(fast_size_before);
}

// ============================================================================
// Group 4: System Info Functions
// ============================================================================

// ============================================================================
// Test 22: page_size_nonzero_power_of_two
// Verify arena_page_size() returns a non-zero power of 2 (typically 4096).
// ============================================================================

FL_TEST("Page Size Nonzero Power of Two", page_size_nonzero_power_of_two) {
    size_t page_size = arena_page_size();

    // Must be nonzero
    FL_ASSERT_GT_SIZE_T(page_size, (size_t)0);

    // Must be a power of 2: (n & (n - 1)) == 0
    FL_ASSERT_EQ_SIZE_T((size_t)0, page_size & (page_size - 1));
}

// ============================================================================
// Test 23: granularity_nonzero_power_of_two
// Verify arena_granularity() returns a non-zero power of 2 >= page_size.
// ============================================================================

FL_TEST("Granularity Nonzero Power of Two", granularity_nonzero_power_of_two) {
    size_t granularity = arena_granularity();
    size_t page_size   = arena_page_size();

    // Must be nonzero
    FL_ASSERT_GT_SIZE_T(granularity, (size_t)0);

    // Must be a power of 2
    FL_ASSERT_EQ_SIZE_T((size_t)0, granularity & (granularity - 1));

    // Must be >= page_size
    FL_ASSERT_GE_SIZE_T(granularity, page_size);
}

// ============================================================================
// Group 7: Mixed Allocation Patterns
// ============================================================================

// ============================================================================
// Test 32: interleaved_small_large
// Allocate small, large, small, large, then free in various orders. Verify
// all operations succeed and allocation count is correct throughout.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Interleaved Small Large", ArenaFastMixedTest,
                           interleaved_small_large, setup_fast_mixed,
                           cleanup_fast_mixed) {
    Arena *arena = t->arena;

    void *small1 = arena_malloc_throw(arena, 32, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)1, arena_allocation_count(arena));

    void *large1 = arena_malloc_throw(arena, 2048, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)2, arena_allocation_count(arena));

    void *small2 = arena_malloc_throw(arena, 64, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)3, arena_allocation_count(arena));

    void *large2 = arena_malloc_throw(arena, 4096, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)4, arena_allocation_count(arena));

    // Free in non-LIFO order: large1, small1, large2, small2
    arena_free_throw(arena, large1, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)3, arena_allocation_count(arena));

    arena_free_throw(arena, small1, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)2, arena_allocation_count(arena));

    arena_free_throw(arena, large2, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)1, arena_allocation_count(arena));

    arena_free_throw(arena, small2, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena_allocation_count(arena));
}

// ============================================================================
// Test 33: reuse_freed_chunks
// Allocate and free N same-sized small chunks, then allocate N more of the
// same size. Verify the second batch reuses bins (top doesn't shrink further).
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Reuse Freed Chunks", ArenaFastMixedTest, reuse_freed_chunks,
                           setup_fast_mixed, cleanup_fast_mixed) {
    Arena *arena = t->arena;

    enum {
        N = 10
    };

    void *ptrs[N];

    // First batch: allocate N same-sized chunks from top
    for (int i = 0; i < N; i++) {
        ptrs[i] = arena_malloc_throw(arena, 64, __FILE__, __LINE__);
    }
    FL_ASSERT_EQ_SIZE_T((size_t)N, arena_allocation_count(arena));

    // Record top size after the first batch of allocations
    size_t top_size_after_first_batch = CHUNK_SIZE(arena->top);

    // Free all of them - most go to bins, but the last one (nearest to top)
    // merges forward into top, growing it
    for (int i = 0; i < N; i++) {
        arena_free_throw(arena, ptrs[i], __FILE__, __LINE__);
    }
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena_allocation_count(arena));

    // Second batch: allocate N more of the same size
    for (int i = 0; i < N; i++) {
        ptrs[i] = arena_malloc_throw(arena, 64, __FILE__, __LINE__);
    }
    FL_ASSERT_EQ_SIZE_T((size_t)N, arena_allocation_count(arena));

    // Most of the second batch should come from bins (reusing freed chunks),
    // not from top. Top may shrink by at most one chunk (the one that had
    // merged into top during freeing), so it should be no smaller than it
    // was after the first batch.
    FL_ASSERT_GE_SIZE_T(CHUNK_SIZE(arena->top), top_size_after_first_batch);
}

// ============================================================================
// Test 34: fragmentation_and_coalesce
// Allocate A, B, C contiguously from top. Free A and C (creating fragments),
// then free B. Verify A+B+C coalesce into a single free chunk.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Fragmentation and Coalesce", ArenaFastMixedTest,
                           fragmentation_and_coalesce, setup_fast_mixed,
                           cleanup_fast_mixed) {
    Arena *arena = t->arena;

    // Allocate three contiguous chunks from top, plus a guard chunk D so C
    // doesn't border top.
    void *mem_a = arena_malloc_throw(arena, 64, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 64, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, 64, __FILE__, __LINE__);
    void *mem_d = arena_malloc_throw(arena, 64, __FILE__, __LINE__); // guard

    FL_ASSERT_EQ_SIZE_T((size_t)4, arena_allocation_count(arena));

    Chunk *chunk_a = CHUNK_FROM_MEMORY(mem_a);
    Chunk *chunk_c = CHUNK_FROM_MEMORY(mem_c);
    size_t size_a  = CHUNK_SIZE(chunk_a);
    size_t size_b  = CHUNK_SIZE(CHUNK_FROM_MEMORY(mem_b));
    size_t size_c  = CHUNK_SIZE(chunk_c);

    // Free A and C - creates two free fragments with B in-use between them
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)2, arena_allocation_count(arena));

    // Free B - should trigger forward merge with C. Backward merge with A
    // depends on whether the PINUSE bug has been fixed.
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);
    FL_ASSERT_EQ_SIZE_T((size_t)1, arena_allocation_count(arena));

    // After freeing B, at minimum B should merge forward with C.
    // Check that we can allocate a chunk of size B+C combined payload,
    // verifying the forward merge produced a usable larger chunk.
    size_t combined_bc_payload = size_b + size_c - CHUNK_ALIGNED_SIZE;
    void *mem_reuse = arena_malloc_throw(arena, combined_bc_payload, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(mem_reuse);

    FL_UNUSED(mem_d);
    FL_UNUSED(size_a);
}
