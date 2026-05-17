/**
 * @file exception_sa_test.c
 * @author Douglas Cuthbertson
 * @brief Standalone test for the FLP/FLA service injection exception architecture.
 *
 * All test cases are defined directly in this file using FL_TEST macros.
 * fl_test.h is a stub that strips FL_TEST/FL_SUITE_* down to plain static
 * function definitions so the tests compile without the FaultLine driver
 * infrastructure.
 *
 * flp_exception_service.c is NOT on the command line — the inline
 * flp_push/pop/throw stubs below provide the driver-side TLS stack and avoid
 * the duplicate fl_throw_assertion symbol that would result from linking both.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty (no dllimport on a locally-defined
 * function).  Use /DDLL_BUILD instead when compiling fla_exception_service.c
 * into an actual DLL.
 * @version 0.1
 * @date 2026-05-10
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fla_exception_service.h> /* FL_TRY, FL_CATCH, ... (plugin side) */
#include <faultline/fl_exception_service.h>  /* FLExceptionService, reason externs  */
#include <faultline/fl_exception_service_assert.h> /* FL_ASSERT_*, FL_FAIL                */
#include <faultline/fl_exception_types.h> /* FLExceptionEnvironment, FL_THROWN   */
#include <faultline/fl_macros.h>          /* FL_THREAD_LOCAL                     */
#include <faultline/fl_test.h>            /* FL_TEST, FL_TEST_SETUP_CLEANUP       */
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Minimal driver-side TLS stack (normally in flp_exception_service.c).
 * Provided here to avoid linking flp_exception_service.c into the test,
 * which would produce a duplicate fl_throw_assertion symbol.
 * ----------------------------------------------------------------------- */
static FL_THREAD_LOCAL FLExceptionEnvironment *g_flp_stack;

static FL_PUSH_EXCEPTION_SERVICE_FN(flp_push) {
    env->next   = g_flp_stack;
    g_flp_stack = env;
}

static FL_POP_EXCEPTION_SERVICE_FN(flp_pop) {
    FLExceptionEnvironment *env = g_flp_stack;
    g_flp_stack                 = env->next;
}

static FL_THROW_EXCEPTION_SERVICE_FN(flp_throw) {
    if (g_flp_stack == NULL) {
        fprintf(stderr, "flp_throw: unhandled exception \"%s\" at %s:%d\n",
                reason ? reason : "(null)", file ? file : "?", line);
        abort();
    }
    FLExceptionEnvironment *env = g_flp_stack;
    g_flp_stack                 = env->next;
    env->reason                 = reason;
    env->details                = details;
    env->file                   = file;
    env->line                   = (uint32_t)line;
    longjmp(env->jmp, FL_THROWN);
}

/* Mirrors flp_init_exception_service(fla_set_exception_service) */
static void init_service(void) {
    FLExceptionService svc = {
        .push_env  = flp_push,
        .pop_env   = flp_pop,
        .throw_exc = flp_throw,
    };
    fla_set_exception_service(&svc, sizeof svc);
}

/* -----------------------------------------------------------------------
 * Minimal harness — wraps each test, counts failures via fl_unexpected_failure
 * ----------------------------------------------------------------------- */
static int g_failures = 0;

static void run(char const *name, void (*fn)(void)) {
    bool success = true;
    FL_TRY {
        fn();
    }
    FL_CATCH_STR(fl_unexpected_failure) {
        fprintf(stderr, "FAIL %s: %s\n", name, FL_DETAILS ? FL_DETAILS : "(no details)");
        g_failures++;
        success = false;
    }
    FL_CATCH_ALL {
        fprintf(stderr, "FAIL %s: unexpected exception \"%s\"\n", name,
                FL_REASON ? FL_REASON : "(null)");
        g_failures++;
        success = false;
    }
    FL_END_TRY;

    if (success) {
        printf("  %s\n", name);
    }
}

/* Tests that intentionally throw fl_expected_failure — treat as pass */
static void run_expected_failure(char const *name, void (*fn)(void)) {
    int threw = 0;
    FL_TRY {
        fn();
    }
    FL_CATCH_STR(fl_expected_failure) {
        threw = 1;
    }
    FL_CATCH_ALL {
        fprintf(stderr, "FAIL %s: expected fl_expected_failure, got \"%s\"\n", name,
                FL_REASON ? FL_REASON : "(null)");
        g_failures++;
    }
    FL_END_TRY;
    if (!threw) {
        fprintf(stderr, "FAIL %s: expected fl_expected_failure but nothing was thrown\n",
                name);
        g_failures++;
    } else {
        printf("  %s\n", name);
    }
}

//
// Local exception reason for tests
//
static FLExceptionReason test_exception = "test exception";
static FLExceptionReason test_reason_a  = "test reason A";
static FLExceptionReason test_reason_b  = "test reason B";

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

int main(void) {
    init_service();
    puts("exception_sa_test");
    run("Throw", test_throw);
    run("Throw File Line", test_throw_file_line);
    run("Throw Details", test_throw_details);
    run("Throw Details File Line", test_throw_details_file_line);
    run("Throw Var Args", test_throw_va);
    run("Throw Var Args File Line", test_throw_va_file_line);
    run("Throw and Rethrow", test_throw_rethrow);
    run("Throw and Catch All", test_throw_catch_all);
    run("Deep Nesting", deep_nesting);
    run("Deep Call Stack", deep_call_stack);
    run("Finally Block", finally_block);
    run("No Throw Path", test_no_throw_path);
    run("Catch Preserves Metadata", test_catch_preserves_metadata);
    run("Finally Without Exception", test_finally_without_exception);
    run("Finally With Unhandled Exception", test_finally_with_unhandled);
    run("Multiple Catch Clauses", test_multiple_catch_clauses);
    run("Throw Details Format Args", test_throw_details_format_args);
    run("Rethrow Preserves Metadata", test_rethrow_preserves_metadata);

    /* These tests exist to verify driver-level expected-failure handling;
     * in the standalone context the thrown fl_expected_failure is the pass condition. */
    run_expected_failure("Expected Test Failure", test_failure);
    run_expected_failure("Expected Test FN", test_expected_failure);

    /* setup_failure and cleanup_failure have no-op bodies in the standalone stub */
    run("Setup Failure (no-op)", setup_failure);
    run("Cleanup Failure (no-op)", cleanup_failure);

    if (g_failures == 0) {
        printf("All exception tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) FAILED.\n", g_failures);
    return 1;
}
