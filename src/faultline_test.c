/**
 * @file faultline_test.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "faultline_test.h"
#include "faultline_test_data.h"
#include <faultline/fl_result_codes.h> // FLResultCode
#include <faultline/fl_context.h>
#include <faultline/fl_log.h>

#include <faultline/fl_test.h>
#include <faultline/fl_try.h>
#include <faultline/fl_abbreviated_types.h>
#include <faultline/fl_macros.h>

// Load data DLL. Note: this must be the name of the DLL for the test data.
// I haven't figured out a way to pass in this name, so it must be hard coded.
#define DRIVER_LIBRARY_STR  "faultline_test_data.dll"
#define DRIVER_LIBRARY_WSTR (L"" DRIVER_LIBRARY_STR)

#define TEST_SUCCESS "Test Success"
#define TEST_FAILURE "Test Failure"

static FLExceptionReason test_throw_failure = "failed to throw test failure";
static FLExceptionReason load_driver_data_failure
    = "failed to load driver library " DRIVER_LIBRARY_STR;
static FLExceptionReason load_initialize_failure = "failed to load " INITIALIZE_CTX_STR;
static FLExceptionReason load_is_valid_failure   = "failed to load " IS_VALID_CTX_STR;
static FLExceptionReason load_begin_failure      = "failed to load " BEGIN_CTX_STR;
static FLExceptionReason load_end_failure        = "failed to load " END_CTX_STR;
static FLExceptionReason load_add_fault_faulure  = "failed to load " ADD_FAULT_CTX_STR;
static FLExceptionReason load_next_failure       = "failed to load " NEXT_CTX_STR;
static FLExceptionReason load_has_more_failure
    = "failed to load " HAS_MORE_CASES_CTX_STR;
static FLExceptionReason load_get_case_name_failure
    = "failed to load " GET_CASE_NAME_CTX_STR;
static FLExceptionReason load_get_result_code_failure
    = "failed to load " GET_RESULT_CODE_CTX_STR;
static FLExceptionReason load_get_index_failure
    = "failed to load " GET_CASE_INDEX_CTX_STR;
static FLExceptionReason load_driver_failure = "failed to load " GET_DRIVER_CTX_STR;
static FLExceptionReason load_get_run_count_failure
    = "failed to load " GET_RUN_COUNT_CTX_STR;
static FLExceptionReason load_get_pass_count_failure
    = "failed to load " GET_PASS_COUNT_CTX_STR;
static FLExceptionReason load_get_fail_count_failure
    = "failed to load " GET_FAIL_COUNT_CTX_STR;
static FLExceptionReason load_get_setup_fail_count_failure
    = {"failed to load " GET_SETUP_FAIL_COUNT_CTX_STR};
static FLExceptionReason load_get_cleanup_fail_count_failure
    = {"failed to load " GET_CLEANUP_FAIL_COUNT_CTX_STR};
static FLExceptionReason load_get_results_count_failure
    = {"failed to load " GET_RESULTS_COUNT_CTX_STR};
static FLExceptionReason clear_test_context_failure
    = "end failed to clear the test context";
static FLExceptionReason invalid_context_failure   = "the context is not valid";
static FLExceptionReason end_of_test_cases_failure = "expected no more test cases";
static FLExceptionReason has_more_failure          = "expected more test cases";
static FLExceptionReason second_index_failure      = "second index is not one";
static FLExceptionReason first_index_failure       = "first index is not zero";
static FLExceptionReason expected_test_case_name_is_not
    = "test case name is not " TEST_SUCCESS;
static FLExceptionReason unexpected_test_case_name_is_not
    = "test case name is not " TEST_FAILURE;
static FLExceptionReason unexpected_index = "unexpected test case index";

static FLExceptionReason expected_one_test_run          = "expected one test run";
static FLExceptionReason expected_one_passing_test      = "expected one passing test";
static FLExceptionReason expected_one_failing_test      = "expected one failing test";
static FLExceptionReason unexpected_setup_failure_count = "expected zero setup failures";
static FLExceptionReason unexpected_cleanup_failure_count
    = "expected zero cleanup failures";
static FLExceptionReason unexpected_test_result_count
    = "expected one non-passing test result";

static FLExceptionReason test_result_phase_mismatch = "test result phase mismatch";
static FLExceptionReason test_result_type_mismatch = "test result failure type mismatch";
static FLExceptionReason test_result_file_missing
    = "test result file information missing";
static FLExceptionReason test_result_line_invalid = "test result line number invalid";

FL_TYPE_TEST(TEST_SUCCESS, TestDriverData, driver_success_test) {
    FL_UNUSED_TYPE_ARG;
}

// This should be caught in test_driver below.
FL_TYPE_TEST(TEST_FAILURE, TestDriverData, driver_failure_test) {
    FL_UNUSED_TYPE_ARG;
    FL_THROW(fl_expected_failure);
}

FL_SUITE_BEGIN(driver_suite)
FL_SUITE_ADD_EMBEDDED(driver_success_test)
FL_SUITE_ADD_EMBEDDED(driver_failure_test)
FL_SUITE_END;

FL_TEST_SUITE("Driver", driver_suite);

static void set_up_test_driver_data(TestDriverData *t) {
    t->h = LoadLibrary(DRIVER_LIBRARY_WSTR);

    if (t->h == NULL) {
        FL_THROW(load_driver_data_failure);
    }

    t->initialize = (faultline_initialize_fn *)GetProcAddress(t->h, INITIALIZE_CTX_STR);
    if (t->initialize == NULL) {
        FL_THROW(load_initialize_failure);
    }

    t->is_valid = (faultline_is_valid_fn *)GetProcAddress(t->h, IS_VALID_CTX_STR);
    if (t->is_valid == NULL) {
        FL_THROW(load_is_valid_failure);
    }

    t->begin = (faultline_begin_fn *)GetProcAddress(t->h, BEGIN_CTX_STR);
    if (t->begin == NULL) {
        FL_THROW(load_begin_failure);
    }

    t->end = (faultline_end_fn *)GetProcAddress(t->h, END_CTX_STR);
    if (t->end == NULL) {
        FL_THROW(load_end_failure);
    }

    t->add_fault = (faultline_add_fault_fn *)GetProcAddress(t->h, ADD_FAULT_CTX_STR);
    if (t->add_fault == NULL) {
        FL_THROW(load_add_fault_faulure);
    }

    t->next = (faultline_next_fn *)GetProcAddress(t->h, NEXT_CTX_STR);
    if (t->next == NULL) {
        FL_THROW(load_next_failure);
    }

    t->has_more = (faultline_has_more_fn *)GetProcAddress(t->h, HAS_MORE_CASES_CTX_STR);
    if (t->has_more == NULL) {
        FL_THROW(load_has_more_failure);
    }

    t->get_case_name
        = (faultline_get_case_name_fn *)GetProcAddress(t->h, GET_CASE_NAME_CTX_STR);
    if (t->get_case_name == NULL) {
        FL_THROW(load_get_case_name_failure);
    }

    t->get_result_code
        = (faultline_get_result_code_fn *)GetProcAddress(t->h, GET_RESULT_CODE_CTX_STR);
    if (t->get_result_code == NULL) {
        FL_THROW(load_get_result_code_failure);
    }

    t->get_index
        = (faultline_get_index_fn *)GetProcAddress(t->h, GET_CASE_INDEX_CTX_STR);
    if (t->get_index == NULL) {
        FL_THROW(load_get_index_failure);
    }

    t->driver = (faultline_driver_fn *)GetProcAddress(t->h, GET_DRIVER_CTX_STR);
    if (t->driver == NULL) {
        FL_THROW(load_driver_failure);
    }

    t->get_run_count
        = (faultline_get_run_count_fn *)GetProcAddress(t->h, GET_RUN_COUNT_CTX_STR);
    if (t->get_run_count == NULL) {
        FL_THROW(load_get_run_count_failure);
    }

    t->get_pass_count
        = (faultline_get_pass_count_fn *)GetProcAddress(t->h, GET_PASS_COUNT_CTX_STR);
    if (t->get_pass_count == NULL) {
        FL_THROW(load_get_pass_count_failure);
    }

    t->get_fail_count
        = (faultline_get_fail_count_fn *)GetProcAddress(t->h, GET_FAIL_COUNT_CTX_STR);
    if (t->get_fail_count == NULL) {
        FL_THROW(load_get_fail_count_failure);
    }

    t->get_setup_fail_count = (faultline_get_setup_fail_count_fn *)
        GetProcAddress(t->h, GET_SETUP_FAIL_COUNT_CTX_STR);
    if (t->get_setup_fail_count == NULL) {
        FL_THROW(load_get_setup_fail_count_failure);
    }

    t->get_cleanup_fail_count = (faultline_get_cleanup_fail_count_fn *)
        GetProcAddress(t->h, GET_CLEANUP_FAIL_COUNT_CTX_STR);
    if (t->get_cleanup_fail_count == NULL) {
        FL_THROW(load_get_cleanup_fail_count_failure);
    }

    t->get_results_count
        = (faultline_get_results_count_fn *)GetProcAddress(t->h,
                                                           GET_RESULTS_COUNT_CTX_STR);
    if (t->get_results_count == NULL) {
        FL_THROW(load_get_results_count_failure);
    }

    t->ts = &driver_suite_ts;
}

static void cleanup_test_driver_data(TestDriverData *t) {
    if (t->h != NULL) {
        FreeLibrary(t->h);
        t->h = NULL;
    }
}

static void setup_context(FLTestCase *tc) {
    TestDriverData *t = FL_CONTAINER_OF(tc, TestDriverData, tc);
    set_up_test_driver_data(t);
    t->arena = new_arena(0, 0);
    t->initialize(&t->context, NULL, t->arena);
    t->begin(&t->context, t->ts);

    if (!t->is_valid(&t->context)) {
        cleanup_test_driver_data(t);
    }
}

static void setup_test_context(FLTestCase *tc) {
    TestDriverData *t = FL_CONTAINER_OF(tc, TestDriverData, tc);
    set_up_test_driver_data(t);
    t->arena = new_arena(0, 0);
    t->initialize(&t->context, NULL, t->arena);
    t->begin(&t->context, t->ts);

    if (!t->is_valid(&t->context)) {
        cleanup_test_driver_data(t);
    }
}

static void cleanup_context(FLTestCase *tc) {
    TestDriverData *t = FL_CONTAINER_OF(tc, TestDriverData, tc);
    if (t->end != NULL) {
        t->end(&t->context);
    }
    cleanup_test_driver_data(t);
    release_arena(&t->arena);
}

FL_TYPE_TEST("Load Driver", TestDriverData, load_driver_test) {
    FL_UNUSED_TYPE_ARG;
    HMODULE library;

    library = LoadLibrary(DRIVER_LIBRARY_WSTR);

    if (library) {
        FreeLibrary(library);
    } else {
        FL_THROW(load_driver_data_failure);
    }
}

// Verify begin() and end() create and delete the test context
FL_TYPE_TEST_SETUP_CLEANUP("Begin and End", TestDriverData, begin_end_test,
                           setup_context, cleanup_context) {
    t->add_fault(&t->context, 0, FL_PASS, FAULT_NO_RESOURCE, "test begin, end", "test",
                 __FILE__, __LINE__);
    if (t->get_results_count(&t->context) != 1) {
        FL_THROW(load_get_results_count_failure);
    }

    t->end(&t->context);
    if (t->get_results_count(&t->context) != 0) {
        FL_THROW(load_get_results_count_failure);
    }
}

// Verify the context is valid
FL_TYPE_TEST_SETUP_CLEANUP("Is Valid", TestDriverData, is_valid_test, setup_context,
                           cleanup_context) {
    if (!t->is_valid(&t->context)) {
        FL_THROW(invalid_context_failure);
    }
    t->end(&t->context);
}

// Verify more()
FL_TYPE_TEST_SETUP_CLEANUP("Next/Index/More", TestDriverData, next_index_test,
                           setup_context, cleanup_context) {
    if (t->get_index(&t->context) == 0) {
        t->next(&t->context);
        if (t->get_index(&t->context) == 1) {
            if (t->has_more(&t->context)) {
                t->next(&t->context);

                // There should be no more test cases
                if (t->has_more(&t->context)) {
                    FL_THROW(end_of_test_cases_failure);
                }
            } else {
                FL_THROW(has_more_failure);
            }
        } else {
            FL_THROW(second_index_failure);
        }
    } else {
        FL_THROW(first_index_failure);
    }
}

// Verify get_case_name returns the correct names of test cases
FL_TYPE_TEST_SETUP_CLEANUP("Case Name", TestDriverData, case_name_test, setup_context,
                           cleanup_context) {
    int         error = 0;
    char const *name;

    name = t->get_case_name(&t->context);

    error = strcmp(name, TEST_SUCCESS);
    if (error != 0) {
        FL_THROW(expected_test_case_name_is_not);
    }

    t->next(&t->context);
    name  = t->get_case_name(&t->context);
    error = strcmp(name, TEST_FAILURE);

    if (error != 0) {
        FL_THROW(unexpected_test_case_name_is_not);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Index", TestDriverData, index_test, setup_context,
                           cleanup_context) {
    size_t index = t->get_index(&t->context);

    if (index != 0) {
        FL_THROW(unexpected_index);
    }

    t->next(&t->context);
    if (t->get_index(&t->context) != 1) {
        FL_THROW(unexpected_index);
    }
}

// static void driver_test(FLTestCase *tc) {
FL_TYPE_TEST_SETUP_CLEANUP("Test the Driver", TestDriverData, test_driver,
                           setup_test_context, cleanup_context) {
    FL_TRY {
        t->driver(&t->context);
        t->next(&t->context);
        t->driver(&t->context);
        // Should have thrown fl_expected_failure to be caught below
        FL_THROW(test_throw_failure);
    }
    FL_CATCH(fl_test_exception) {
        // Catch and update counters. Increment tests_failed to ensure the pass-count is
        // correct.
        t->context.tests_failed++;
        t->add_fault(&t->context, t->context.index, FL_FAIL, FAULT_NO_RESOURCE,
                     FL_REASON, FL_DETAILS, __FILE__, __LINE__);
    }
    // If anything else was thrown, it's a failure. Let the test driver catch it.
    FL_END_TRY;

    size_t actual = t->get_run_count(&t->context);
    if (actual != 1) {
        FL_THROW_DETAILS(expected_one_test_run, "Expected 1. Actual %zu", actual);
    }

    actual = t->get_pass_count(&t->context);
    if (actual != 1) {
        FL_THROW_DETAILS(expected_one_passing_test, "Expected 1. Actual %zu", actual);
    }

    if (t->get_fail_count(&t->context) != 1) {
        FL_THROW(expected_one_failing_test);
    }

    if (t->get_setup_fail_count(&t->context) != 0) {
        FL_THROW(unexpected_setup_failure_count);
    }

    if (t->get_cleanup_fail_count(&t->context) != 0) {
        FL_THROW(unexpected_cleanup_failure_count);
    }

    if (t->get_results_count(&t->context) != 1) {
        FL_THROW(unexpected_test_result_count);
    }

    // Retrieve the result code for test case 1 (it's 1, because we called
    // "t->next(&t->context);" above).
    FLResultCode code = t->get_result_code(&t->context, t->context.index);
    if (code != FL_FAIL) { // we caught an expected test-failure above
        FL_THROW_DETAILS(fl_internal_error, "Expected code: %d. Actual: %d", FL_FAIL,
                         code);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Results", TestDriverData, results_test, setup_test_context,
                           cleanup_context) {
    t->driver(&t->context);
    t->next(&t->context);
    FL_TRY {
        t->driver(&t->context);
        // Should have failed with fl_expected_failure, so this should not be reached.
        FL_THROW(fl_test_exception);
    }
    FL_CATCH(fl_expected_failure) {
        ; // Catch and release
    }
    FL_END_TRY;
}

FL_TYPE_TEST_SETUP_CLEANUP("Discovery Failure Recording", TestDriverData,
                           discovery_failure_recording, setup_test_context,
                           cleanup_context) {
    // Create a test summary and properly set its phase and failure type
    FLTestSummary *summary
        = faultline_test_summary_buffer_allocate_next_free_slot(&t->context.results);
    init_faultline_test_summary(summary, t->context.arena, 0, FL_FAIL,
                                "Discovery phase test failure", NULL);

    // Use the phase info function to properly set discovery phase failure
    faultline_update_summary_phase_info(summary, FL_DISCOVERY_PHASE, FL_TEST_FAILURE,
                                        0.001);

    // Add a fault to the summary
    faultline_test_summary_add_fault(summary, 0, FL_FAIL, FAULT_NO_RESOURCE,
                                     "Discovery phase test failure", "test", __FILE__,
                                     __LINE__);

    if (t->get_results_count(&t->context) != 1) {
        FL_THROW(unexpected_test_result_count);
    }

    if (summary->failure_phase != FL_DISCOVERY_PHASE) {
        FL_THROW(test_result_phase_mismatch);
    }

    if (summary->failure_type != FL_TEST_FAILURE) {
        FL_THROW(test_result_type_mismatch);
    }

    // Validate that fault information was recorded
    if (fault_buffer_count(&summary->fault_buffer) == 0) {
        FL_THROW(test_result_file_missing);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Injection Failure Recording", TestDriverData,
                           injection_failure_recording, setup_test_context,
                           cleanup_context) {
    // Create a test summary and properly set its phase and failure type for injection
    // phase
    FLTestSummary *summary
        = faultline_test_summary_buffer_allocate_next_free_slot(&t->context.results);
    init_faultline_test_summary(summary, t->context.arena, 5, FL_FAIL,
                                "Injection phase malloc failure", NULL);

    // Use the phase info function to properly set injection phase failure
    faultline_update_summary_phase_info(summary, FL_INJECTION_PHASE, FL_TEST_FAILURE,
                                        0.002);

    // Add a fault to the summary
    faultline_test_summary_add_fault(summary, 5, FL_FAIL, FAULT_NO_RESOURCE,
                                     "Injection phase malloc failure", "test", __FILE__,
                                     __LINE__);

    if (t->get_results_count(&t->context) != 1) {
        FL_THROW(unexpected_test_result_count);
    }

    if (summary->failure_phase != FL_INJECTION_PHASE) {
        FL_THROW(test_result_phase_mismatch);
    }

    // Check that fault was recorded with correct index
    if (fault_buffer_count(&summary->fault_buffer) == 0) {
        FL_THROW(fl_internal_error);
    }

    Fault *fault = fault_buffer_get(&summary->fault_buffer, 0);
    if (fault->index != 5) {
        FL_THROW_DETAILS(fl_internal_error, "Expected fault site 5, got %lld",
                         fault->index);
    }

    if (summary->failure_type != FL_TEST_FAILURE) {
        FL_THROW(test_result_type_mismatch);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Result Validation", TestDriverData, result_validation,
                           setup_test_context, cleanup_context) {
    FL_UNUSED_TYPE_ARG;
    TestResult result = {0};

    // TestResult doesn't have phase field - it's in FLTestSummary
    result.failure_type = FL_TEST_FAILURE;
    result.file         = __FILE__;
    result.line         = __LINE__;
    result.reason       = "Test validation";
    result.rc           = FL_FAIL;
    result.unexpected_exception
        = true; // This is required for validation to check failure fields

    if (!faultline_validate_test_result(&result)) {
        FL_THROW("Valid test result marked as invalid");
    }

    // Test an invalid result - zero-initialized with unexpected_exception = true but
    // missing required fields The validation function will log warnings for this invalid
    // result, which is expected behavior
    TestResult invalid_result = {0};
    invalid_result.unexpected_exception
        = true; // Set this so validation checks the failure fields

    // The validation will log warnings for this invalid result, which is expected
    // behavior We can't suppress individual warnings without affecting the whole logger,
    // so we'll accept them
    bool is_valid = faultline_validate_test_result(&invalid_result);

    if (is_valid) {
        FL_THROW("Invalid test result marked as valid");
    }
}
