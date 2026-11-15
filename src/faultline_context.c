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
#include <faultline/fl_result_codes.h>      // for FLResultCode
#include <faultline/fl_test_summary.h>      // for faultline_test_summary_buffer_get
#include <faultline/fl_types.h>             // for FLFailureType, FaultlineT...
#include <faultline/fl_abbreviated_types.h> // for i64
#include <faultline/fl_exception.h>         // for fl_internal_error
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

        // The scan may have run off the end, so check the bound before fetching:
        // buffer_get throws on an out-of-range index.
        FLTestSummary *result
            = (i < buffer_count(&fctx->results))
                  ? faultline_test_summary_buffer_get(&fctx->results, i)
                  : NULL;
        if (result != NULL && result->index == index) {
            // We have a match
            frc = result->code;
        } else {
            // There's no result, so if it ran it passed
            frc = FL_PASS;
        }
    }

    return frc;
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
