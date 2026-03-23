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
            LOG_ERROR_FILE_LINE("Test Failure", file, line, test_case,
                                "%s: Unexpected Exception: %s: %s", test_case, reason,
                                details);
        } else {
            LOG_ERROR_FILE_LINE("Test Failure", file, line, test_case,
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
    BasicResultCode result = BUT_RC_PASSED;
    FLTestCase     *tc     = bctx->ts->test_cases[bctx->index];

    bctx->run_count++;

    if (tc == NULL) {
        result = BUT_RC_FAILED;
        new_result(bctx, BUT_RC_FAILED, invalid_test_case, __FILE__, __LINE__);
        bctx->test_failures++;
        FL_THROW_DETAILS(invalid_test_case, "test case %zu does not exist", bctx->index);
    }

    FL_TRY {
        if (tc->setup != NULL) {
            tc->setup(tc);
        }
    }
    FL_CATCH_ALL {
        result = BUT_RC_FAILED_SETUP;
        if (FL_UNEXPECTED_EXCEPTION(FL_REASON)) {
            new_result(bctx, BUT_RC_FAILED_SETUP, FL_REASON, FL_FILE, FL_LINE);
            bctx->setup_failures++;
            FL_RETHROW;
        }
    }
    FL_END_TRY;

    if (result == BUT_RC_PASSED) {
        FL_TRY {
            tc->test(tc);
        }
        FL_CATCH_ALL {
            if (FL_UNEXPECTED_EXCEPTION(FL_REASON)) {
                // log the unexpected failure and continue
                FLExceptionReason reason  = FL_REASON;
                char const       *details = FL_DETAILS;
                char const       *file    = FL_FILE;
                int               line    = FL_LINE;
                new_result(bctx, BUT_RC_FAILED, reason, file, line);
                bctx->test_failures++;
                bd_log_error(tc->name, reason, details, file, line);
            }
        }
        FL_END_TRY;
    }

    FL_TRY {
        if (tc->cleanup != NULL) {
            tc->cleanup(tc);
        }
    }
    FL_CATCH_ALL {
        if (FL_UNEXPECTED_EXCEPTION(FL_REASON)) {
            new_result(bctx, BUT_RC_FAILED_CLEANUP, FL_REASON, FL_FILE, FL_LINE);
            bctx->cleanup_failures++;
            FL_RETHROW;
        }
        // Expected failure - don't rethrow, just continue
    }
    FL_END_TRY;
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
