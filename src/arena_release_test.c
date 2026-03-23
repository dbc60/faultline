/**
 * @file arena_release_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for arena_free_throw() coalescing paths.
 * @version 0.1
 * @date 2026-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Tests all 10 merge/no-merge paths through arena_free_throw():
 *   1. No merge (both neighbors in-use)
 *   2. Forward merge with top
 *   3. Forward merge with fast node
 *   4. Forward merge with small-bin chunk
 *   5. Forward merge with large-bin chunk (known bug)
 *   6. Backward merge with fast node (known PINUSE bug)
 *   7. Backward merge with small-bin chunk (known PINUSE bug)
 *   8. Backward merge with large-bin chunk (known PINUSE bug)
 *   9. Merge both directions (known PINUSE bug)
 *  10. Merge both directions into top (known PINUSE bug)
 */
// Test framework
#include <faultline/fl_test.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/fl_macros.h>

// ============================================================================
// Test fixture
// ============================================================================

typedef struct ArenaReleaseTest {
    FLTestCase tc;
    Arena     *arena;
} ArenaReleaseTest;

static FL_SETUP_FN(setup_release) {
    ArenaReleaseTest *t = FL_CONTAINER_OF(tc, ArenaReleaseTest, tc);
    t->arena            = new_arena(MEBI(1), 0);
}

static FL_CLEANUP_FN(cleanup_release) {
    ArenaReleaseTest *t = FL_CONTAINER_OF(tc, ArenaReleaseTest, tc);
    release_arena(&t->arena);
}

// ============================================================================
// Test 1: release_no_merge
// Both neighbors are in-use, so no merge occurs.
// Allocate A, B, C, D (D is a guard so C doesn't border top).
// Release C -> goes to a small bin.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: No Merge", ArenaReleaseTest, release_no_merge,
                           setup_release, cleanup_release) {
    Arena *arena = t->arena;

    // Allocate four small chunks
    void *mem_a = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_d = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // guard

    FL_ASSERT_EQ_SIZE_T((size_t)4, arena->allocations);

    Chunk *chunk_a = CHUNK_FROM_MEMORY(mem_a);
    Chunk *chunk_b = CHUNK_FROM_MEMORY(mem_b);
    Chunk *chunk_c = CHUNK_FROM_MEMORY(mem_c);
    Chunk *chunk_d = CHUNK_FROM_MEMORY(mem_d);

    FL_ASSERT_TRUE(CHUNK_IS_INUSE(chunk_a));
    FL_ASSERT_TRUE(CHUNK_IS_INUSE(chunk_b));
    FL_ASSERT_TRUE(CHUNK_IS_INUSE(chunk_c));
    FL_ASSERT_TRUE(CHUNK_IS_INUSE(chunk_d));

    size_t c_size = CHUNK_SIZE(chunk_c);
    u32    c_idx  = ARENA_SMALL_INDEX(c_size);

    // Release C: neighbors B and D are still in-use, so no merge
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);

    FL_ASSERT_EQ_SIZE_T((size_t)3, arena->allocations);
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(chunk_c));
    FL_ASSERT_TRUE(CHUNK_IS_INUSE(chunk_b));
    FL_ASSERT_TRUE(CHUNK_IS_INUSE(chunk_d));

    // C should be in a small bin
    FL_ASSERT_TRUE(ARENA_SMALL_MAP_IS_MARKED(arena, c_idx) != 0);

    // Cleanup remaining allocations
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);
    arena_free_throw(arena, mem_d, __FILE__, __LINE__);
}

// ============================================================================
// Test 2: release_forward_merge_top
// Next chunk is arena->top.
// Allocate A (leaving top as next). Release A -> merges into top.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Forward Merge Top", ArenaReleaseTest,
                           release_forward_merge_top, setup_release, cleanup_release) {
    Arena *arena             = t->arena;
    size_t original_top_size = CHUNK_SIZE(arena->top);

    // Allocate one chunk from top
    void  *mem_a   = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    Chunk *chunk_a = CHUNK_FROM_MEMORY(mem_a);

    FL_ASSERT_EQ_SIZE_T((size_t)1, arena->allocations);
    FL_ASSERT_TRUE(CHUNK_IS_INUSE(chunk_a));

    // Next chunk after A should be top
    FL_ASSERT_EQ_PTR((void *)CHUNK_NEXT(chunk_a), (void *)arena->top);

    // Release A -> should merge forward into top
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);

    FL_ASSERT_EQ_SIZE_T((size_t)0, arena->allocations);
    // Top should have moved back to A's position
    FL_ASSERT_EQ_PTR((void *)chunk_a, (void *)arena->top);
    // Top size should be restored
    FL_ASSERT_EQ_SIZE_T(original_top_size, CHUNK_SIZE(arena->top));
}

// ============================================================================
// Test 3: release_forward_merge_fast
// Next chunk is the fast node.
// Strategy: put a chunk into a small bin, then allocate a smaller request
// from that bin (via allocate_from_next_nonempty_small_bin), which creates
// a fast node as the remainder. Then release the allocation right before
// the fast node.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Forward Merge Fast", ArenaReleaseTest,
                           release_forward_merge_fast, setup_release, cleanup_release) {
    Arena *arena = t->arena;

    // Step 1: Allocate three chunks from top to get in-use chunks.
    // A is a guard at the front.
    void *mem_a = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 48, __FILE__, __LINE__); // chunk size 64
    void *mem_c = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // guard after B

    // Step 2: Release B to put it in a small bin (size 64, index 2)
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);

    Chunk *chunk_b = CHUNK_FROM_MEMORY(mem_b);
    u32    b_idx   = ARENA_SMALL_INDEX(CHUNK_SIZE(chunk_b));
    FL_ASSERT_TRUE(ARENA_SMALL_MAP_IS_MARKED(arena, b_idx) != 0);

    // Step 3: Allocate a size-16 request. The fast node is empty (size 0),
    // so no exact fit for 32 (bin 0 is empty), no fast node, so it will go to
    // allocate_from_next_nonempty_small_bin which splits B's bin (size 64) chunk
    // into a 32-byte allocation + 32-byte fast node.
    void *mem_d = arena_malloc_throw(arena, 16, __FILE__, __LINE__);

    // mem_d should be at the same address as the old B
    FL_ASSERT_EQ_PTR(mem_b, mem_d);

    // Now there should be a fast node (the remainder from splitting B)
    FL_ASSERT_NOT_NULL(arena->fast);
    FL_ASSERT_GT_SIZE_T(arena->fast_size, (size_t)0);

    FreeChunk *fast      = arena->fast;
    size_t     fast_size = arena->fast_size;

    // The chunk we just allocated (mem_d) should be right before the fast node
    Chunk *chunk_d = CHUNK_FROM_MEMORY(mem_d);
    FL_ASSERT_EQ_PTR((void *)CHUNK_NEXT(chunk_d), (void *)fast);

    size_t d_size = CHUNK_SIZE(chunk_d);

    // Step 4: Release D -> should forward merge with fast node
    arena_free_throw(arena, mem_d, __FILE__, __LINE__);

    // After merge, fast should point to D's position (merged chunk)
    FL_ASSERT_EQ_PTR((void *)chunk_d, (void *)arena->fast);
    // Fast size should be D's size + old fast size
    FL_ASSERT_EQ_SIZE_T(d_size + fast_size, arena->fast_size);

    // Cleanup
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);
}

// ============================================================================
// Test 4: release_forward_merge_small_bin
// Next chunk is in a small bin.
// Allocate A, B, C, D (guard). Release C (goes to bin). Release B -> merges
// forward with C.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Forward Merge Small Bin", ArenaReleaseTest,
                           release_forward_merge_small_bin, setup_release,
                           cleanup_release) {
    Arena *arena = t->arena;

    // Allocate four chunks
    void *mem_a = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_d = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // guard

    Chunk *chunk_b = CHUNK_FROM_MEMORY(mem_b);
    Chunk *chunk_c = CHUNK_FROM_MEMORY(mem_c);

    size_t b_size = CHUNK_SIZE(chunk_b);
    size_t c_size = CHUNK_SIZE(chunk_c);

    // Release C -> goes to small bin (neighbors B and D are in-use)
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);

    u32 c_idx = ARENA_SMALL_INDEX(c_size);
    FL_ASSERT_TRUE(ARENA_SMALL_MAP_IS_MARKED(arena, c_idx) != 0);
    FL_ASSERT_EQ_SIZE_T((size_t)3, arena->allocations);

    // Release B -> should forward merge with C
    // B's next (C) is free and in a small bin, so C gets unlinked and merged
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);

    FL_ASSERT_EQ_SIZE_T((size_t)2, arena->allocations);

    // The merged chunk should have size b_size + c_size
    FreeChunk *merged = (FreeChunk *)chunk_b;
    FL_ASSERT_EQ_SIZE_T(b_size + c_size, CHUNK_SIZE(merged));
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(merged));

    // Cleanup
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);
    arena_free_throw(arena, mem_d, __FILE__, __LINE__);
}

// ============================================================================
// Test 5: release_forward_merge_large_bin
// Next chunk is in a large bin.
// Known bug: dst_remove_leftmost return value discarded; bin pointer and
// large_map not updated.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Forward Merge Large Bin", ArenaReleaseTest,
                           release_forward_merge_large_bin, setup_release,
                           cleanup_release) {
    Arena *arena = t->arena;

    // Allocate: small A, large B (request 1008 -> chunk 1024), then C as guard
    void *mem_a = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 1008, __FILE__, __LINE__); // chunk size 1024
    void *mem_c = arena_malloc_throw(arena, 16, __FILE__, __LINE__);   // guard

    Chunk *chunk_a = CHUNK_FROM_MEMORY(mem_a);
    Chunk *chunk_b = CHUNK_FROM_MEMORY(mem_b);

    size_t a_size = CHUNK_SIZE(chunk_a);
    size_t b_size = CHUNK_SIZE(chunk_b);

    FL_ASSERT_GE_SIZE_T(b_size, (size_t)ARENA_MIN_LARGE_CHUNK);
    FL_ASSERT_EQ_SIZE_T((size_t)3, arena->allocations);

    // Release B -> goes to a large bin (neighbors A and C are in-use)
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);

    FL_ASSERT_EQ_SIZE_T((size_t)2, arena->allocations);
    FL_ASSERT_TRUE(arena->large_map != 0);

    // Record B's large bin index before releasing A
    u32 b_large_idx;
    INDEX_BY_VALUE64(b_size, ARENA_LARGE_BIN_COUNT, ARENA_LOG2_MIN_LARGE_CHUNK,
                     b_large_idx);

    // Release A -> should forward merge with B (large bin chunk)
    // This tests the forward-merge-with-large-bin path at arena.c lines 897-899
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);

    FL_ASSERT_EQ_SIZE_T((size_t)1, arena->allocations);

    // The merged chunk should have size a_size + b_size
    FreeChunk *merged = (FreeChunk *)chunk_a;
    FL_ASSERT_EQ_SIZE_T(a_size + b_size, CHUNK_SIZE(merged));
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(merged));

    // The large bin should point to the merged chunk (at chunk_a's address),
    // NOT to the stale B address. Before the fix, *bin still pointed to chunk_b.
    size_t merged_size = CHUNK_SIZE(merged);
    u32    merged_idx;
    INDEX_BY_VALUE64(merged_size, ARENA_LARGE_BIN_COUNT, ARENA_LOG2_MIN_LARGE_CHUNK,
                     merged_idx);
    DigitalSearchTree **bin = ARENA_LARGE_BIN_AT(arena, merged_idx);
    FL_ASSERT_EQ_PTR((void *)*bin, (void *)merged);

    // The old bin entry for B should have been properly cleaned up.
    // If B's index differs from the merged chunk's index, B's bin should be empty.
    if (b_large_idx != merged_idx) {
        DigitalSearchTree **old_bin = ARENA_LARGE_BIN_AT(arena, b_large_idx);
        FL_ASSERT_NULL(*old_bin);
        FL_ASSERT_FALSE(ARENA_LARGE_MAP_IS_MARKED(arena, b_large_idx));
    }

    // Cleanup
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);
}

// ============================================================================
// Test 6: release_backward_merge_fast
// Previous chunk is the fast node.
// After the backward merge the merged chunk goes into a small bin. The fast
// node must not simultaneously still point to the merged chunk (mutual
// exclusion invariant).
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Backward Merge Fast", ArenaReleaseTest,
                           release_backward_merge_fast, setup_release, cleanup_release) {
    Arena *arena = t->arena;

    // Step 1: Create a fast node by splitting a small-bin chunk.
    // Layout goal: guard | D(32) | fast(32) | C(32) | E(32,guard) | top
    void *mem_guard = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // front guard
    void *mem_b     = arena_malloc_throw(arena, 48, __FILE__, __LINE__); // chunk 64
    void *mem_c     = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // after B
    void *mem_e     = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // guard after C

    // Release B to put it into a small bin
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);

    // Allocate 16 bytes: splits B into 32 (returned) + 32 (fast node)
    void *mem_d = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    FL_ASSERT_EQ_PTR(mem_b, mem_d);

    FL_ASSERT_NOT_NULL(arena->fast);
    FreeChunk *fast      = arena->fast;
    size_t     fast_size = arena->fast_size;

    // The fast node sits between D and C
    Chunk *chunk_c = CHUNK_FROM_MEMORY(mem_c);
    size_t c_size  = CHUNK_SIZE(chunk_c);

    // Verify the fast node is right before C
    FL_ASSERT_EQ_PTR((void *)CHUNK_NEXT(fast), (void *)chunk_c);

    // Step 2: Release C. Its previous chunk (fast) is free, so backward merge
    // SHOULD happen. E is in-use guard so no forward merge with top.
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);

    // The merged chunk (fast+C) must be either in a small bin OR the fast node,
    // never both simultaneously.
    size_t merged_size = fast_size + c_size;
    u32    merged_idx  = ARENA_SMALL_INDEX(merged_size);
    if (ARENA_SMALL_MAP_IS_MARKED(arena, merged_idx)) {
        // Merged chunk went into a bin - fast must not still point to it.
        FL_ASSERT_NULL(arena->fast);
    } else {
        // Merged chunk stayed as the fast node.
        FL_ASSERT_EQ_PTR((void *)fast, (void *)arena->fast);
        FL_ASSERT_EQ_SIZE_T(merged_size, arena->fast_size);
    }

    // Cleanup
    arena_free_throw(arena, mem_guard, __FILE__, __LINE__);
    arena_free_throw(arena, mem_d, __FILE__, __LINE__);
    arena_free_throw(arena, mem_e, __FILE__, __LINE__);
}

// ============================================================================
// Test 7: release_backward_merge_small_bin
// Previous chunk is in a small bin.
// Known PINUSE bug: backward merge won't trigger because PINUSE is still set.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Backward Merge Small Bin", ArenaReleaseTest,
                           release_backward_merge_small_bin, setup_release,
                           cleanup_release) {
    Arena *arena = t->arena;

    // Allocate A, B, C where C is a guard
    void *mem_a = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // guard

    Chunk *chunk_a = CHUNK_FROM_MEMORY(mem_a);
    Chunk *chunk_b = CHUNK_FROM_MEMORY(mem_b);

    size_t a_size = CHUNK_SIZE(chunk_a);
    size_t b_size = CHUNK_SIZE(chunk_b);

    // Release A -> goes to small bin (B and the base are its neighbors)
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);

    // B's previous (A) is now free. When we release B, backward merge SHOULD happen.
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);

    // If backward merge worked, A and B should be merged
    FreeChunk *merged = (FreeChunk *)chunk_a;
    FL_ASSERT_EQ_SIZE_T(a_size + b_size, CHUNK_SIZE(merged));
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(merged));

    // Cleanup
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);
}

// ============================================================================
// Test 8: release_backward_merge_large_bin
// Previous chunk is in a large bin.
// Known PINUSE bug: backward merge won't trigger.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Backward Merge Large Bin", ArenaReleaseTest,
                           release_backward_merge_large_bin, setup_release,
                           cleanup_release) {
    Arena *arena = t->arena;

    // Allocate large A (request 1008 -> chunk 1024), small B, guard C
    void *mem_a = arena_malloc_throw(arena, 1008, __FILE__, __LINE__); // chunk 1024
    void *mem_b = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // guard

    Chunk *chunk_a = CHUNK_FROM_MEMORY(mem_a);
    Chunk *chunk_b = CHUNK_FROM_MEMORY(mem_b);

    size_t a_size = CHUNK_SIZE(chunk_a);
    size_t b_size = CHUNK_SIZE(chunk_b);

    FL_ASSERT_GE_SIZE_T(a_size, (size_t)ARENA_MIN_LARGE_CHUNK);

    // Release A -> goes to large bin (B is in-use, and base is before A)
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);

    FL_ASSERT_TRUE(arena->large_map != 0);

    // Release B -> backward merge with A SHOULD happen
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);

    // If backward merge worked, A and B should be merged
    FreeChunk *merged = (FreeChunk *)chunk_a;
    FL_ASSERT_EQ_SIZE_T(a_size + b_size, CHUNK_SIZE(merged));
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(merged));

    // Cleanup
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);
}

// ============================================================================
// Test 9: release_merge_both
// Both neighbors are free (backward + forward merge).
// Known PINUSE bug: backward merge won't trigger, only forward.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Merge Both", ArenaReleaseTest, release_merge_both,
                           setup_release, cleanup_release) {
    Arena *arena = t->arena;

    // Allocate A, B, C, D where D is guard
    void *mem_a = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_c = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_d = arena_malloc_throw(arena, 16, __FILE__, __LINE__); // guard

    Chunk *chunk_a = CHUNK_FROM_MEMORY(mem_a);
    Chunk *chunk_b = CHUNK_FROM_MEMORY(mem_b);
    Chunk *chunk_c = CHUNK_FROM_MEMORY(mem_c);

    size_t a_size = CHUNK_SIZE(chunk_a);
    size_t b_size = CHUNK_SIZE(chunk_b);
    size_t c_size = CHUNK_SIZE(chunk_c);

    // Release A and C -> both go to small bins
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);
    arena_free_throw(arena, mem_c, __FILE__, __LINE__);

    // Release B -> should merge backward with A AND forward with C
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);

    // If both merges worked, the combined chunk starts at A and covers A+B+C
    FreeChunk *merged = (FreeChunk *)chunk_a;
    FL_ASSERT_EQ_SIZE_T(a_size + b_size + c_size, CHUNK_SIZE(merged));
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(merged));

    // Cleanup
    arena_free_throw(arena, mem_d, __FILE__, __LINE__);
}

// ============================================================================
// Test 10: release_merge_both_to_top
// Previous is free, next is top. Should merge backward with A and forward
// into top.
// Known PINUSE bug: backward merge with A won't trigger, but forward merge
// with top will.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Release: Merge Both to Top", ArenaReleaseTest,
                           release_merge_both_to_top, setup_release, cleanup_release) {
    Arena *arena = t->arena;

    size_t original_top_size = CHUNK_SIZE(arena->top);

    // Allocate A and B (B borders top)
    void *mem_a = arena_malloc_throw(arena, 16, __FILE__, __LINE__);
    void *mem_b = arena_malloc_throw(arena, 16, __FILE__, __LINE__);

    Chunk *chunk_a = CHUNK_FROM_MEMORY(mem_a);
    Chunk *chunk_b = CHUNK_FROM_MEMORY(mem_b);

    // Verify B borders top
    FL_ASSERT_EQ_PTR((void *)CHUNK_NEXT(chunk_b), (void *)arena->top);

    // Release A -> goes to small bin
    arena_free_throw(arena, mem_a, __FILE__, __LINE__);

    // Release B -> should merge backward with A AND forward with top
    arena_free_throw(arena, mem_b, __FILE__, __LINE__);

    FL_ASSERT_EQ_SIZE_T((size_t)0, arena->allocations);

    // If both merges worked, top should be back at A's position with original size
    FL_ASSERT_EQ_PTR((void *)chunk_a, (void *)arena->top);
    FL_ASSERT_EQ_SIZE_T(original_top_size, CHUNK_SIZE(arena->top));
}
