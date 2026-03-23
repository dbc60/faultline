/**
 * @file but_test_cases.c
 * @author Douglas Cuthbertson
 * @brief Test cases for the Basic Unit Test (BUT) library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "but_test_cases.h" // TestDriverData

#include <faultline/fl_exception_types.h> // fl_exception_handler_fn, FLExceptionReason
#include <faultline/fl_exception_service.h>
#include <faultline/fl_exception_service_assert.h> // assert macros and fl_unexpected_failure declaration
#include <faultline/fl_macros.h>                   // FL_CONTAINER_OF
#include <faultline/fl_try.h>                      // FLA_TRY, FLA_CATCH, etc.
#include <faultline/fl_log.h>                      // LOG_* macros

// We have to import the API defined in but_test_data.h
#undef FL_DLL_BUILD
#include "but_test_data.h" // SET_CONTEXT, IS_VALID_CTX_STR, etc.
#define FL_DLL_BUILD

#include <stddef.h> // size_t, NULL
#include <stdio.h>  // printf

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h> // HMODULE, LoadLibrary, FreeLibrary

// Load data DLL. Note: this must be the name of the DLL for the test data.
// I haven't figured out a way to pass in this name, so it must be hard coded.
#define DRIVER_LIBRARY_STR  "but_test_data.dll"
#define DRIVER_LIBRARY_WSTR (L"" DRIVER_LIBRARY_STR)

#define TEST_SUCCESS "Success"
#define TEST_FAILURE "Failure"

static void set_up_context(FLTestCase *tc);
static void set_up_test_context(FLTestCase *tc);
static void cleanup_context(FLTestCase *tc);

static void set_up_test_driver_data(TestDriverData *tc);
static void cleanup_test_driver_data(TestDriverData *tc);

FL_TYPE_TEST(TEST_SUCCESS, TestDriverData, success) {
    FL_UNUSED_TYPE_ARG;
    return; // This test always succeeds.
}

// The failure should be caught in test() below.
FL_TYPE_TEST(TEST_FAILURE, TestDriverData, failure) {
    FL_UNUSED_TYPE_ARG;
    FL_THROW(fl_test_exception);
}

FL_SUITE_BEGIN(driver_test_data)
FL_SUITE_ADD_EMBEDDED(success)
FL_SUITE_ADD_EMBEDDED(failure)
FL_SUITE_END;

FL_TEST_SUITE("Driver Data", driver_test_data);

FL_TYPE_TEST("Load Driver", TestDriverData, load_driver) {
    FL_UNUSED_TYPE_ARG;
    HMODULE library = LoadLibrary(DRIVER_LIBRARY_WSTR);

    FL_ASSERT_TRUE(library != 0);
    FreeLibrary(library);
}

static void set_up_test_driver_data(TestDriverData *tdd) {
    tdd->h = LoadLibrary(DRIVER_LIBRARY_WSTR);
    FL_ASSERT_NOT_NULL(tdd->h);

    tdd->set_service
        = (fla_set_exception_service_fn *)GetProcAddress(tdd->h,
                                                         FLA_SET_EXCEPTION_SERVICE_STR);
    FL_ASSERT_NOT_NULL(tdd->set_service);

    tdd->is_valid = (bd_is_valid_fn *)GetProcAddress(tdd->h, IS_VALID_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->is_valid);

    tdd->begin = (bd_begin_fn *)GetProcAddress(tdd->h, BEGIN_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->begin);

    tdd->end = (bd_end_fn *)GetProcAddress(tdd->h, END_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->end);

    tdd->next = (bd_next_fn *)GetProcAddress(tdd->h, NEXT_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->next);

    tdd->more = (bd_has_more_fn *)GetProcAddress(tdd->h, MORE_CASES_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->more);

    tdd->get_test_case_name
        = (bd_get_test_case_name_fn *)GetProcAddress(tdd->h, GET_CASE_NAME_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->get_test_case_name);

    tdd->get_index = (bd_get_index_fn *)GetProcAddress(tdd->h, GET_CASE_INDEX_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->get_index);

    tdd->test = (bd_driver_fn *)GetProcAddress(tdd->h, RUN_CURRENT_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->test);

    tdd->get_pass_count
        = (bd_get_pass_count_fn *)GetProcAddress(tdd->h, GET_PASS_COUNT_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->get_pass_count);

    tdd->get_fail_count
        = (bd_get_test_failure_count_fn *)GetProcAddress(tdd->h, GET_FAIL_COUNT_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->get_fail_count);

    tdd->get_failed_set_up_count
        = (bd_get_setup_failure_count_fn *)GetProcAddress(tdd->h,
                                                          GET_SETUP_FAIL_COUNT_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->get_failed_set_up_count);

    tdd->get_results_count
        = (bd_get_results_count_fn *)GetProcAddress(tdd->h, GET_RESULTS_COUNT_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->get_results_count);

    tdd->get_result = (bd_get_result_fn *)GetProcAddress(tdd->h, GET_RESULT_CTX_STR);
    FL_ASSERT_NOT_NULL(tdd->get_result);

    tdd->ts = &FL_TEST_SUITE_NAME(driver_test_data);
}

static void cleanup_test_driver_data(TestDriverData *tdd) {
    if (tdd->h != NULL) {
        FreeLibrary(tdd->h);
        tdd->h = NULL;
    }
}

static void set_up_context(FLTestCase *tc) {
    TestDriverData *tdd = FL_CONTAINER_OF(tc, TestDriverData, tc);
    set_up_test_driver_data(tdd);
    fl_exception_handler_fn *handler
        = (fl_exception_handler_fn *)GetProcAddress(tdd->h, "test_data_handler");
    if (handler == NULL) {
        FL_THROW("failed to load test_data_handler");
    }
    tdd->begin(&tdd->context, tdd->ts);

    if (!tdd->is_valid(&tdd->context)) {
        cleanup_test_driver_data(tdd);
        FL_THROW("set_up_context is invalid");
    }
}

static void set_up_test_context(FLTestCase *tc) {
    TestDriverData *tdd = FL_CONTAINER_OF(tc, TestDriverData, tc);
    set_up_test_driver_data(tdd);
    tdd->begin(&tdd->context, tdd->ts);

    if (!tdd->is_valid(&tdd->context)) {
        cleanup_test_driver_data(tdd);
    }

    // Inject exception service for FL_TRY in test data DLL
    if (tdd->set_service) {
        tdd->set_service(&tdd->exception_service, sizeof tdd->exception_service);
    }
}

static void cleanup_context(FLTestCase *tc) {
    TestDriverData *tdd = FL_CONTAINER_OF(tc, TestDriverData, tc);
    tdd->end(&tdd->context);
    cleanup_test_driver_data(tdd);
}

// Verify begin() and end() create and delete the test context
FL_TYPE_TEST_SETUP_CLEANUP("Begin and End", TestDriverData, begin_end, set_up_context,
                           cleanup_context) {
    t->end(&t->context);
    FL_ASSERT_NULL(t->context.results);
}

// Verify the context is valid
FL_TYPE_TEST_SETUP_CLEANUP("Is Valid", TestDriverData, is_valid, set_up_context,
                           cleanup_context) {
    FL_ASSERT_TRUE(t->is_valid(&t->context));
    t->end(&t->context);
}

// Verify get_index(), more(), and next() work as expected
FL_TYPE_TEST_SETUP_CLEANUP("Next/Index/More", TestDriverData, next_index, set_up_context,
                           cleanup_context) {
    // verify the first index is zero
    FL_ASSERT_TRUE(t->get_index(&t->context) == 0);

    // verify next() advances the index to 1
    t->next(&t->context);
    FL_ASSERT_EQ_SIZE_T(t->get_index(&t->context), (size_t)1);

    // verify more() returns true
    FL_ASSERT_TRUE(t->more(&t->context));

    t->next(&t->context);
    // There should be no more test cases
    FL_ASSERT_FALSE(t->more(&t->context));
}

// Verify get_test_case_name returns the correct names of test cases
FL_TYPE_TEST_SETUP_CLEANUP("Case Name", TestDriverData, case_name, set_up_context,
                           cleanup_context) {
    char const *name;

    // The first test case should be "Success"
    name = t->get_test_case_name(&t->context);
    FL_ASSERT_STR_EQ(name, TEST_SUCCESS);

    // Advance to the next test case
    t->next(&t->context);

    // The second test case should be "Failure"
    name = t->get_test_case_name(&t->context);
    FL_ASSERT_STR_EQ(name, TEST_FAILURE);
}

FL_TYPE_TEST_SETUP_CLEANUP("Case Index", TestDriverData, case_index, set_up_context,
                           cleanup_context) {
    size_t index;

    // The first test case should have an index of 0
    index = t->get_index(&t->context);
    FL_ASSERT_TRUE(index == 0);

    // Advance to the next test case
    t->next(&t->context);

    // The second test case should have an index of 1
    index = t->get_index(&t->context);
    FL_ASSERT_EQ_SIZE_T(index, (size_t)1);
}

FL_TYPE_TEST_SETUP_CLEANUP("Test the Driver", TestDriverData, test, set_up_test_context,
                           cleanup_context) {
    // Verify the test case is valid
    FL_ASSERT_TRUE(t->is_valid(&t->context));

    FL_TRY {
        t->test(&t->context);
        t->next(&t->context);
        t->test(&t->context);
        // Should have thrown bd_expected_failure to be caught below
        FL_THROW(fl_internal_error);
    }
    FL_CATCH(fl_test_exception) {
        // Catch and update counters
        t->context.test_failures++;
        t->context.results_count++;
        t->context.run_count++;
    }
    // If anything else was thrown, it's a failure. Let the test driver catch it.
    FL_END_TRY;

    FL_ASSERT_EQ_SIZE_T(t->get_pass_count(&t->context), (size_t)1);
    FL_ASSERT_EQ_SIZE_T(t->get_fail_count(&t->context), (size_t)1);
    FL_ASSERT_TRUE(t->get_failed_set_up_count(&t->context) == 0);
    FL_ASSERT_EQ_SIZE_T(t->context.results_count, (size_t)1);
}

FL_TYPE_TEST_SETUP_CLEANUP("Results", TestDriverData, results, set_up_test_context,
                           cleanup_context) {
    t->test(&t->context);
    t->next(&t->context);
    FL_TRY {
        t->test(&t->context);
        // Should have failed
        FL_THROW(fl_test_exception);
    }
    FL_CATCH(fl_test_exception) {
        ; // Catch and release
    }
    FL_END_TRY;
}
