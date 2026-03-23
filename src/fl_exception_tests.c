/**
 * @file fla_exception_test.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the exception handling library using FL_TRY macros.
 * @version 0.2
 * @date 2026-01-31
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include <faultline/fl_test.h>               // FL_TEST
#include <faultline/fl_try.h>                // FL_TRY, FL_CATCH, FL_THROW, etc.
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT macros and fl_unexpected_failure declaration
#include <faultline/fl_exception_types.h>          // FLExceptionReason

#include <stddef.h> // NULL

//
// Local exception reason for tests
//
static FLExceptionReason test_exception = "test exception";
static FLExceptionReason test_reason_a  = "test reason A";
static FLExceptionReason test_reason_b  = "test reason B";

//
// Test cases using FL_TRY macros
//

FL_TEST("Throw", test_throw) {
    FL_TRY {
        FL_THROW(test_exception);
    }
    FL_CATCH(test_exception) {
        ; // Success
    }
    FL_END_TRY;
}

FL_TEST("Throw File Line", test_throw_file_line) {
    FL_TRY {
        FL_THROW_FILE_LINE(test_exception, __FILE__, __LINE__);
    }
    FL_CATCH(test_exception) {
        ; // Success
    }
    FL_END_TRY;
}

FL_TEST("Throw Details", test_throw_details) {
    FL_TRY {
        FL_THROW_DETAILS(test_exception, "throw details test");
    }
    FL_CATCH(test_exception) {
        ; // Success
    }
    FL_END_TRY;
}

FL_TEST("Throw Details File Line", test_throw_details_file_line) {
    FL_TRY {
        FL_THROW_DETAILS_FILE_LINE(test_exception,
                                   "test throw details with file and line number",
                                   __FILE__, __LINE__);
    }
    FL_CATCH(test_exception) {
        ; // Success
    }
    FL_END_TRY;
}

FL_TEST("Throw Var Args", test_throw_va) {
    FL_TRY {
        FL_THROW_DETAILS(test_exception, "test");
    }
    FL_CATCH(test_exception) {
        ; // Success
    }
    FL_END_TRY;
}

FL_TEST("Throw Var Args File Line", test_throw_va_file_line) {
    FL_TRY {
        FL_THROW_DETAILS_FILE_LINE(test_exception, "test", "source file", 0);
    }
    FL_CATCH(test_exception) {
        ; // Success
    }
    FL_END_TRY;
}

FL_TEST("Throw and Rethrow", test_throw_rethrow) {
    FL_TRY {
        FL_TRY {
            FL_THROW(test_exception);
        }
        FL_CATCH_ALL {
            FL_RETHROW;
        }
        FL_END_TRY;
    }
    FL_CATCH(test_exception) {
        ; // Success
    }
    FL_END_TRY;
}

FL_TEST("Throw and Catch All", test_throw_catch_all) {
    FL_TRY {
        FL_TRY {
            FL_THROW(test_exception);
        }
        FL_CATCH(fl_invalid_value) {
            FL_FAIL("Should not catch here");
        }
        FL_END_TRY;
    }
    FL_CATCH_ALL {
        ; // should catch here
    }
    FL_END_TRY;
}

/**
 * @brief throw an explicit exception that will be caught by the test driver.
 */
FL_TEST_FN(test_expected_failure) {
    FL_TEST_UNUSED;
    FL_THROW(fl_expected_failure);
}

FL_TEST_SETUP_CLEANUP("Expected Test Failure", test_failure, fl_default_setup,
                      fl_default_cleanup) {
    FL_THROW(fl_expected_failure);
}

FL_TEST_SETUP_CLEANUP("Expected Test Setup Failure", setup_failure,
                      test_expected_failure, fl_default_cleanup) {
    // no-op
    ;
}

FL_TEST_SETUP_CLEANUP("Expected Test Setup Failure", cleanup_failure, fl_default_setup,
                      test_expected_failure) {
    ;
}

/**
 * @brief Throw from multiple nested contexts to ensure it is caught correctly.
 */
FL_TEST("Deep Nesting", deep_nesting) {
    FL_TRY {
        FL_TRY {
            FL_TRY {
                FL_TRY {
                    FL_THROW(fl_test_exception);
                }
                FL_END_TRY;
            }
            FL_END_TRY;
        }
        FL_END_TRY;
    }
    FL_CATCH(fl_test_exception) {
        ; // Success - exception propagated through all levels
    }
    FL_END_TRY;
}

static void call3(void) {
    FL_THROW(fl_test_exception);
}

static void call2(void) {
    call3();
}

static void call1(void) {
    call2();
}

static void call0(void) {
    call1();
}

/**
 * @brief Ensure exception handling works correctly with deep call stacks.
 */
FL_TEST("Deep Call Stack", deep_call_stack) {
    FL_TRY {
        call0();
    }
    FL_CATCH(fl_test_exception) {
        ; // Success - exception caught
    }
    FL_END_TRY;
}

FL_TEST("Finally Block", finally_block) {
    int finally_called = 0;
    int catch_called   = 0;

    FL_TRY {
        FL_THROW(fl_test_exception);
    }
    FL_CATCH(fl_test_exception) {
        catch_called = 1;
    }
    FL_FINALLY {
        finally_called = 2;
    }
    FL_END_TRY;

    FL_ASSERT_EQ_INT(catch_called, 1);
    FL_ASSERT_EQ_INT(finally_called, 2);
}

//
// Coverage tests
//

FL_TEST("No Throw Path", test_no_throw_path) {
    int entered = 0;
    FL_TRY {
        entered = 1;
    }
    FL_END_TRY;
    FL_ASSERT_EQ_INT(entered, 1);
}

FL_TEST("Catch Preserves Metadata", test_catch_preserves_metadata) {
    FL_TRY {
        FL_THROW_DETAILS(test_exception, "meta value %d", 99);
    }
    FL_CATCH(test_exception) {
        FL_ASSERT_TRUE(FL_REASON == test_exception);
        FL_ASSERT_NOT_NULL(FL_DETAILS);
        FL_ASSERT_TRUE(strstr(FL_DETAILS, "meta value 99") != NULL);
        FL_ASSERT_NOT_NULL(FL_FILE);
        FL_ASSERT_TRUE(FL_LINE > 0);
    }
    FL_END_TRY;
}

FL_TEST("Finally Without Exception", test_finally_without_exception) {
    int finally_called = 0;
    FL_TRY {
        ; // no throw
    }
    FL_FINALLY {
        finally_called = 1;
    }
    FL_END_TRY;
    FL_ASSERT_EQ_INT(finally_called, 1);
}

FL_TEST("Finally With Unhandled Exception", test_finally_with_unhandled) {
    // finally_called must be volatile, because it's set between the setjmp calls in the
    // FL_TRY clauses and the RETHROW (longjmp) in the first FL_END_TRY clause. See
    // section §7.13.2.1 ¶3 of the C11 spec for details.
    int volatile finally_called = 0;
    FL_TRY {
        FL_TRY {
            FL_THROW(test_exception);
        }
        FL_FINALLY {
            finally_called = 1;
        }
        FL_END_TRY;
    }
    FL_CATCH(test_exception) {
        ; // caught after the finally block above ran
    }
    FL_END_TRY;
    FL_ASSERT_EQ_INT(finally_called, 1);
}

FL_TEST("Multiple Catch Clauses", test_multiple_catch_clauses) {
    int caught_b = 0;
    FL_TRY {
        FL_THROW(test_reason_b);
    }
    FL_CATCH(test_reason_a) {
        FL_FAIL("should not catch A");
    }
    FL_CATCH(test_reason_b) {
        caught_b = 1;
    }
    FL_END_TRY;
    FL_ASSERT_EQ_INT(caught_b, 1);
}

FL_TEST("Throw Details Format Args", test_throw_details_format_args) {
    FL_TRY {
        FL_THROW_DETAILS(test_exception, "val=%d str=%s", 42, "hello");
    }
    FL_CATCH(test_exception) {
        FL_ASSERT_NOT_NULL(FL_DETAILS);
        FL_ASSERT_TRUE(strstr(FL_DETAILS, "val=42") != NULL);
        FL_ASSERT_TRUE(strstr(FL_DETAILS, "str=hello") != NULL);
    }
    FL_END_TRY;
}

FL_TEST("Rethrow Preserves Metadata", test_rethrow_preserves_metadata) {
    FL_TRY {
        FL_TRY {
            FL_THROW_DETAILS(test_exception, "rethrow detail");
        }
        FL_CATCH_ALL {
            FL_RETHROW;
        }
        FL_END_TRY;
    }
    FL_CATCH(test_exception) {
        FL_ASSERT_TRUE(FL_REASON == test_exception);
        FL_ASSERT_NOT_NULL(FL_DETAILS);
        FL_ASSERT_TRUE(strstr(FL_DETAILS, "rethrow detail") != NULL);
    }
    FL_END_TRY;
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD(test_throw)
FL_SUITE_ADD(test_throw_file_line)
FL_SUITE_ADD(test_throw_details)
FL_SUITE_ADD(test_throw_details_file_line)
FL_SUITE_ADD(test_throw_va)
FL_SUITE_ADD(test_throw_va_file_line)
FL_SUITE_ADD(test_throw_rethrow)
FL_SUITE_ADD(test_throw_catch_all)
FL_SUITE_ADD(test_failure)
FL_SUITE_ADD(setup_failure)
FL_SUITE_ADD(cleanup_failure)
FL_SUITE_ADD(deep_nesting)
FL_SUITE_ADD(deep_call_stack)
FL_SUITE_ADD(finally_block)
FL_SUITE_ADD(test_no_throw_path)
FL_SUITE_ADD(test_catch_preserves_metadata)
FL_SUITE_ADD(test_finally_without_exception)
FL_SUITE_ADD(test_finally_with_unhandled)
FL_SUITE_ADD(test_multiple_catch_clauses)
FL_SUITE_ADD(test_throw_details_format_args)
FL_SUITE_ADD(test_rethrow_preserves_metadata)
FL_SUITE_END;

FL_GET_TEST_SUITE("Exception Service", ts);
