/**
 * @file faultline_context.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-12-29
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_context.h>
#include <faultline/buffer.h>               // for buffer_count
#include <faultline/fault.h>                // for FAULT_NO_RESOURCE
#include <faultline/fl_result_codes.h>      // for FLResultCode
#include <faultline/fl_test_summary.h>      // for faultline_test_summary_buffer_get
#include <faultline/fl_types.h>             // for FLFailureType, FaultlineT...
#include <faultline/fl_abbreviated_types.h> // for i64
#include <faultline/fl_exception_service.h> // for fl_internal_error
#include <faultline/fl_test.h>              // for FLTestSuite, FLTestCase
#include <faultline/fl_try.h>               // for FL_THROW
#include <stdbool.h>                        // for bool, false, true
#include "faultline_test_result.h"          // for TestResult

// Get the name of the current test case
FL_GET_CASE_NAME(faultline_get_case_name) {
    char const  *name = 0;
    FLTestSuite *ts   = fctx->ts;

    if (fctx->index < fctx->count) {
        name = ts->test_cases[fctx->index]->name;
    } else {
        // test case index out of range
        FL_THROW(fl_internal_error);
    }

    return name;
}

// Get the result code from the result context of a test case
FL_GET_RESULT_CODE(faultline_get_result_code) {
    FLResultCode frc;
    size_t       i;

    if (buffer_count(&fctx->results) == 0) {
        // No results were generated, so all tests passed
        frc = FL_PASS;
    } else {
        // Search the results and see if there's one matching the index
        i = 0;
        while (i < buffer_count(&fctx->results)
               && faultline_test_summary_buffer_get(&fctx->results, i)->index < index) {
            i++;
        }

        FLTestSummary *result = faultline_test_summary_buffer_get(&fctx->results, i);
        if (i < buffer_count(&fctx->results) && result->index == index) {
            // We have a match
            frc = result->code;
        } else {
            // There's no result, so if it ran it passed
            frc = FL_PASS;
        }
    }

    return frc;
}

/**
 * @brief Standardized function to record injection phase failures with complete context
 *
 * This ensures all injection phase failures are recorded consistently with proper
 * fault index correlation and complete failure information.
 *
 * @param summary the test summary to update
 * @param fault_index the fault injection index where failure occurred
 * @param result_code the FLResultCode from the failed operation
 * @param failure_type the categorized type of failure
 * @param test_result the TestResult containing detailed failure context (can be NULL for
 * synthetic failures)
 * @param phase_time the time spent in this injection phase (seconds)
 * @param synthetic_reason reason string for synthetic failures (leaks, invalid frees)
 * when test_result is NULL
 * @param fallback_file fallback file name when no better context is available
 * @param fallback_line fallback line number when no better context is available
 */
void faultline_record_injection_failure(FLTestSummary *summary, i64 fault_index,
                                        FLResultCode  result_code,
                                        FLFailureType failure_type,
                                        TestResult *test_result, double phase_time,
                                        char const *synthetic_reason,
                                        char const *details, char const *fallback_file,
                                        int fallback_line) {
    // Update summary with injection phase failure information
    faultline_update_summary_phase_info(summary, FL_INJECTION_PHASE, failure_type,
                                        phase_time);

    // Determine the best available context information
    char const *reason = NULL;
    char const *file   = NULL;
    int         line   = 0;

    if (test_result && test_result->unexpected_exception) {
        // Use real test failure context (highest priority)
        reason = test_result->reason;
        file   = test_result->file;
        line   = test_result->line;
    } else if (synthetic_reason) {
        // Use synthetic failure with best available context
        reason = synthetic_reason;

        // Try to get file/line from test_result even if it didn't have an exception
        if (test_result && test_result->file) {
            file = test_result->file;
            line = test_result->line;
        } else {
            // Fall back to provided context
            file = fallback_file;
            line = fallback_line;
        }
    }

    // Ensure we always have some context
    if (!file) {
        file = "unknown";
        line = 0;
    }
    if (!reason) {
        reason = "unspecified failure";
    }

    // Record the fault with validated context
    faultline_test_summary_add_fault(summary, fault_index, result_code,
                                     FAULT_NO_RESOURCE, reason, details, file, line);
}

bool faultline_validate_test_result(TestResult *test_result) {
    if (!test_result) {
        return false;
    }

    // Only validate failure information if there's supposed to be a failure
    if (!test_result->unexpected_exception) {
        return true; // Successful tests don't need failure details
    }

    // For failed tests, all failure context must be present
    return test_result->reason && test_result->file && test_result->line > 0
           && test_result->failure_type != FL_FAILURE_NONE;
}
