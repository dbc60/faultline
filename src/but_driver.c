/**
 * @file but_driver.c
 * @author Douglas Cuthbertson
 * @brief Manage context information for the Basic Unit Test (BUT) library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "but_driver.h"
#include <faultline/fl_exception_types.h>   // for FLExceptionReason
#include <faultline/fl_try.h>               // for FL_CATCH_ALL, FL_END_TRY, FL_TRY
#include <faultline/fl_test.h>              // for FLTestCase, FLTestSuite, FL_UNEXP...
#include <stdbool.h>                        // for true, bool, false
#include <stddef.h>                         // NULL
#include <stdlib.h>                         // for free
#include "but_context.h"                    // for BasicResultCode
#include "but_result_context.h"             // for new_result, ResultContext
#include <faultline/fl_abbreviated_types.h> // for u32
#include <faultline/fl_exception_service.h> // for FL_REASON, FL_FILE, FL_LINE, FL_D...
#include <faultline/fl_log.h>               // for LOG_ERROR
#include "intrinsics_win32.h"               // for memset

static FLExceptionReason invalid_test_case = "invalid test case";

// Check the validity of the test context
BD_IS_VALID(bd_is_valid) {
    bool valid = false;

    if (bctx) {
        if (bctx->initialized && bctx->index <= bctx->test_case_count) {
            valid = true;
        }
    }

    return valid;
}

BD_LOG_ERROR(bd_log_error) {
    if (file != NULL) {
        if (details != NULL) {
            LOG_ERROR_FILE_LINE("Test Failure", file, line,
                                "%s: Unexpected Exception: %s: %s", test_case, reason,
                                details);
        } else {
            LOG_ERROR_FILE_LINE("Test Failure", file, line,
                                "%s: Unexpected Exception: %s", test_case, reason);
        }
    } else {
        if (details != NULL) {
            LOG_ERROR("Test Failure", "%s: Unexpected Exception: %s: %s", test_case,
                      reason, details);
        } else {
            LOG_ERROR("Test Failure", "%s: Unexpected Exception: %s", test_case, reason);
        }
    }
}

// assign a test suite to a test context
BD_BEGIN(bd_begin) {
    memset(bctx, 0, sizeof *bctx);
    bctx->initialized     = true;
    bctx->ts              = bts;
    bctx->run_case        = run_case;
    bctx->test_case_count = bts->count;
}

// release the memory resources allocated during testing
BD_END(bd_end) {
    if (bctx->results) {
        free(bctx->results);
        bctx->results = NULL;
    }
}

// Move to the next test case
BD_NEXT(bd_next) {
    if (bctx->index < bctx->test_case_count) {
        bctx->index++;
    }
}

// Return true if there are more test cases
BD_HAS_MORE(bd_has_more) {
    return bctx->index < bctx->test_case_count;
}

// Get the name of the current test case
BD_GET_TEST_CASE_NAME(bd_get_test_case_name) {
    char const  *name;
    FLTestSuite *bts = bctx->ts;

    if (bctx->index >= 0 && bctx->index < bctx->test_case_count) {
        name = bts->test_cases[bctx->index]->name;
    } else {
        name = "test case index out of range";
    }

    return name;
}

// Get the index of the current test case
BD_GET_INDEX(bd_get_index) {
    return bctx->index;
}

// Execute the current test case
BD_DRIVER(bd_driver) {
    FLTestCase   *tc = bctx->ts->test_cases[bctx->index];
    FLCaseOutcome out;

    bctx->run_count++;

    if (tc == NULL) {
        new_result(bctx, BUT_RC_FAILED, invalid_test_case, __FILE__, __LINE__);
        bctx->test_failures++;
        FL_THROW_DETAILS(invalid_test_case, "test case %zu does not exist", bctx->index);
    }

    FLRunCaseResult ran = bctx->run_case(bctx->index, &out, sizeof out);
    if (ran != FL_RUN_CASE_OK) {
        new_result(bctx, BUT_RC_FAILED, fl_run_case_result_str(ran), __FILE__, __LINE__);
        bctx->test_failures++;
        return;
    }

    // An expected failure is not a failure, and the module reports it as its own
    // status, so only an unexpected one is recorded here.
    if (out.status != FL_CASE_UNEXPECTED_FAILURE) {
        return;
    }

    switch (out.failure_type) {
    case FL_SETUP_FAILURE:
        new_result(bctx, BUT_RC_FAILED_SETUP, out.reason, out.file, out.line);
        bctx->setup_failures++;
        break;
    case FL_CLEANUP_FAILURE:
        new_result(bctx, BUT_RC_FAILED_CLEANUP, out.reason, out.file, out.line);
        bctx->cleanup_failures++;
        break;
    case FL_TEST_FAILURE:
        new_result(bctx, BUT_RC_FAILED, out.reason, out.file, out.line);
        bctx->test_failures++;
        bd_log_error(tc->name, out.reason, out.details[0] != '\0' ? out.details : NULL,
                     out.file, out.line);
        break;
    case FL_FAILURE_NONE:
    case FL_LEAK_FAILURE:
    case FL_INVALID_FREE_FAILURE:
    default:
        // fl_run_case reports failures for only the three phases above (setup, test, and
        // cleanup), so any other value means the module and this driver disagree about
        // FLFailureType.
        new_result(bctx, BUT_RC_FAILED, out.reason, out.file, out.line);
        bctx->test_failures++;
        break;
    }
}

// Get the number of test cases executed
BD_GET_RUN_COUNT(bd_get_run_count) {
    return bctx->run_count;
}

// Get the number of test cases that passed
BD_GET_PASS_COUNT(bd_get_pass_count) {
    return bctx->test_case_count
           - (bctx->test_failures + bctx->setup_failures + bctx->cleanup_failures);
}

// Get the number of test cases that failed
BD_GET_TEST_FAILURE_COUNT(bd_get_test_failure_count) {
    return bctx->test_failures;
}

// Get the number of test cases where setup failed
BD_GET_SETUP_FAILURE_COUNT(bd_get_setup_failure_count) {
    return bctx->setup_failures;
}

// Get the number of test cases that failed in the cleanup phase
BD_GET_CLEANUP_FAILURE_COUNT(bd_get_cleanup_failure_count) {
    return bctx->cleanup_failures;
}

// Get the number of result contexts
BD_GET_RESULTS_COUNT(bd_get_results_count) {
    return bctx->results_count;
}

// Get the result code from the result context of a test case
BD_GET_RESULT(bd_get_result) {
    BasicResultCode bd_result;
    u32             i;

    if (bctx->results_count == 0) {
        // No results were generated, so all tests passed
        bd_result = BUT_RC_PASSED;
    } else {
        // Search the results and see if there's one matching the index
        i = 0;
        while (i < bctx->results_count && bctx->results[i].index < index) {
            i++;
        }

        if (i < bctx->results_count && bctx->results[i].index == index) {
            // We have a match
            bd_result = bctx->results[i].status;
        } else {
            // There's no result, so if it ran it passed
            bd_result = BUT_RC_PASSED;
        }
    }

    return bd_result;
}
