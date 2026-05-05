/**
 * @file arena_catch_str_test.c
 * @author Douglas Cuthbertson
 * @brief Tests for FL_CATCH_STR.
 * @version 0.1
 * @date 2026-05-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * FL_CATCH_STR matches exception reasons by strcmp rather than by pointer
 * identity.  This is necessary when the same reason string is statically
 * linked into multiple modules: each module gets its own copy of the variable
 * at a distinct address, so FL_CATCH (pointer equality) would silently fail to
 * match an exception thrown by a different module.
 *
 * Tests:
 *  E. catch_str_catches_matching_string   - FL_CATCH_STR matches equal strings
 *  F. catch_str_skips_non_matching_string - FL_CATCH_STR skips different strings
 *  G. catch_str_vs_pointer_identity       - demonstrates the cross-module scenario:
 *                                           FL_CATCH misses a same-string/different-
 *                                           address reason; FL_CATCH_STR catches it
 */
#include <faultline/fl_test.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/fl_macros.h>

// Private reason used exclusively by this test file.
static FLExceptionReason catch_str_reason_a = "catch_str test reason A";

// A second variable whose *content* matches catch_str_reason_a but whose
// *address* differs.  This simulates the same reason string being defined
// independently in two different modules (e.g., a driver and a test DLL).
static FLExceptionReason catch_str_reason_a_copy = "catch_str test reason A";

static FLExceptionReason catch_str_reason_b = "catch_str test reason B";

// ============================================================================
// Test E: catch_str_catches_matching_string
// Throw a reason; FL_CATCH_STR with the identical string must catch it.
// ============================================================================

FL_TEST("FL_CATCH_STR catches matching string", catch_str_catches_matching_string) {
    bool caught = false;

    FL_TRY {
        FL_THROW(catch_str_reason_a);
    }
    FL_CATCH_STR(catch_str_reason_a) {
        caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(caught);
}

// ============================================================================
// Test F: catch_str_skips_non_matching_string
// Throw reason A; FL_CATCH_STR with reason B must not catch it.
// A subsequent FL_CATCH_ALL must catch the uncaught exception.
// ============================================================================

FL_TEST("FL_CATCH_STR skips non-matching string", catch_str_skips_non_matching_string) {
    bool caught_b   = false;
    bool caught_all = false;

    FL_TRY {
        FL_THROW(catch_str_reason_a);
    }
    FL_CATCH_STR(catch_str_reason_b) {
        caught_b = true;
    }
    FL_CATCH_ALL {
        caught_all = true;
    }
    FL_END_TRY;

    FL_ASSERT_FALSE(caught_b);
    FL_ASSERT_TRUE(caught_all);
}

// ============================================================================
// Test G: catch_str_vs_pointer_identity
// Demonstrates the cross-module scenario.
//
// catch_str_reason_a and catch_str_reason_a_copy have the same string content
// but live at different addresses (two independent static variables).
//
// Phase 1: FL_CATCH(catch_str_reason_a_copy) does NOT catch an exception thrown
//          with catch_str_reason_a because the addresses differ.
// Phase 2: FL_CATCH_STR(catch_str_reason_a_copy) DOES catch it because strcmp
//          compares the contents, not the addresses.
// ============================================================================

FL_TEST("FL_CATCH_STR matches same-string different-address reason",
        catch_str_vs_pointer_identity) {
    // Verify the addresses truly differ (the test premise).
    FL_ASSERT_DETAILS(catch_str_reason_a != catch_str_reason_a_copy,
                      "expected distinct addresses for the two reason variables");

    // Phase 1: FL_CATCH uses pointer equality — it misses the cross-address case.
    bool pointer_caught = false;
    FL_TRY {
        FL_THROW(catch_str_reason_a);
    }
    FL_CATCH(catch_str_reason_a_copy) {
        pointer_caught = true;
    }
    FL_CATCH_ALL {
        // The exception falls here because FL_CATCH did not match.
    }
    FL_END_TRY;

    FL_ASSERT_FALSE(pointer_caught);

    // Phase 2: FL_CATCH_STR uses strcmp — it correctly catches the exception.
    bool str_caught = false;
    FL_TRY {
        FL_THROW(catch_str_reason_a);
    }
    FL_CATCH_STR(catch_str_reason_a_copy) {
        str_caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(str_caught);
}
