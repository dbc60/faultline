/**
 * @file arena_footprint_limit_test.c
 * @author Douglas Cuthbertson
 * @brief Tests for arena_set_footprint_limit.
 * @version 0.1
 * @date 2026-05-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Tests:
 *  A. footprint_limit_rounds_up       - limit stored is rounded to granularity
 *  B. footprint_limit_enforced        - allocation requiring growth throws when at limit
 *  C. footprint_limit_zero_means_unlimited - limit=0 removes a previously set limit
 *  D. footprint_limit_clear_restores_growth - clearing limit allows growth again
 */
#include <faultline/fl_test.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/fl_macros.h>
#include <faultline/arena_malloc.h> // arena_out_of_memory
#include <faultline/size.h>         // MEBI

// ============================================================================
// Test fixture
// ============================================================================

typedef struct ArenaLimitTest {
    FLTestCase tc;
    Arena     *arena;
} ArenaLimitTest;

static FL_SETUP_FN(setup_limit) {
    ArenaLimitTest *t = FL_CONTAINER_OF(tc, ArenaLimitTest, tc);
    t->arena          = new_arena(0, 0);
}

static FL_CLEANUP_FN(cleanup_limit) {
    ArenaLimitTest *t = FL_CONTAINER_OF(tc, ArenaLimitTest, tc);
    release_arena(&t->arena);
}

// ============================================================================
// Test A: footprint_limit_rounds_up
// Pass a limit that is not granularity-aligned; verify the stored value is
// rounded up to the next granularity boundary.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Footprint Limit Rounds Up", ArenaLimitTest,
                           footprint_limit_rounds_up, setup_limit, cleanup_limit) {
    Arena *arena       = t->arena;
    size_t granularity = arena_granularity();

    // Pick a limit that is exactly 1 byte past the current footprint so it is
    // not granularity-aligned (the footprint itself is always aligned).
    size_t requested = arena_footprint(arena) + 1;
    arena_set_footprint_limit(arena, requested);

    size_t expected = ALIGN_UP(requested, granularity);
    FL_ASSERT_EQ_SIZE_T(expected, arena->footprint_limit);

    // The stored limit must be a multiple of granularity.
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena->footprint_limit % granularity);
}

// ============================================================================
// Test B: footprint_limit_enforced
// Set the limit to the current footprint (preventing any growth), then attempt
// an allocation large enough to require an extension.  Verify it throws
// arena_out_of_memory.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Footprint Limit Enforced", ArenaLimitTest,
                           footprint_limit_enforced, setup_limit, cleanup_limit) {
    Arena *arena = t->arena;

    // Cap the arena at its current committed size.
    arena_set_footprint_limit(arena, arena_footprint(arena));

    bool threw = false;
    FL_TRY {
        // A 1 MiB request will require growing beyond the initial commit.
        arena_malloc_throw(arena, MEBI(1), __FILE__, __LINE__);
    }
    FL_CATCH(arena_out_of_memory) {
        threw = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(threw);
}

// ============================================================================
// Test C: footprint_limit_zero_means_unlimited
// Calling arena_set_footprint_limit(arena, 0) stores zero and removes any
// growth constraint.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Footprint Limit Zero Means Unlimited", ArenaLimitTest,
                           footprint_limit_zero_means_unlimited, setup_limit,
                           cleanup_limit) {
    Arena *arena = t->arena;

    // Apply a tight limit to confirm the mechanism works.
    arena_set_footprint_limit(arena, arena_footprint(arena));
    FL_ASSERT_GT_SIZE_T(arena->footprint_limit, (size_t)0);

    // Clear the limit.
    arena_set_footprint_limit(arena, 0);
    FL_ASSERT_EQ_SIZE_T((size_t)0, arena->footprint_limit);

    // A large allocation must now succeed.
    bool threw = false;
    FL_TRY {
        void *mem = arena_malloc_throw(arena, MEBI(1), __FILE__, __LINE__);
        arena_free_throw(arena, mem, __FILE__, __LINE__);
    }
    FL_CATCH(arena_out_of_memory) {
        threw = true;
    }
    FL_END_TRY;

    FL_ASSERT_FALSE(threw);
}

// ============================================================================
// Test D: footprint_limit_clear_restores_growth
// End-to-end: set a tight limit, confirm it blocks growth, clear it, confirm
// growth works again.
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Footprint Limit Clear Restores Growth", ArenaLimitTest,
                           footprint_limit_clear_restores_growth, setup_limit,
                           cleanup_limit) {
    Arena *arena = t->arena;

    arena_set_footprint_limit(arena, arena_footprint(arena));

    // First attempt must throw.
    bool threw = false;
    FL_TRY {
        arena_malloc_throw(arena, MEBI(1), __FILE__, __LINE__);
    }
    FL_CATCH(arena_out_of_memory) {
        threw = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(threw);

    // Remove the limit.
    arena_set_footprint_limit(arena, 0);

    // Second attempt must succeed.
    threw = false;
    FL_TRY {
        void *mem = arena_malloc_throw(arena, MEBI(1), __FILE__, __LINE__);
        arena_free_throw(arena, mem, __FILE__, __LINE__);
    }
    FL_CATCH(arena_out_of_memory) {
        threw = true;
    }
    FL_END_TRY;
    FL_ASSERT_FALSE(threw);
}
