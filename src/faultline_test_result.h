#ifndef FAULTLINE_TEST_RESULT_H_
#define FAULTLINE_TEST_RESULT_H_

/**
 * @file faultline_test_result.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.2.0
 * @date 2025-12-12
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_exception_types.h> // FL_MAX_DETAILS_LENGTH
#include <faultline/fl_result_codes.h>
#include <faultline/fl_types.h>

#include <stdbool.h> // bool
#include <stdio.h>   // snprintf
#include <string.h>  // for memset

typedef struct TestResult {
    char const   *reason;
    char const   *details; ///< NULL, or points at details_buf below
    char const   *file;
    int           line;
    FLResultCode  rc;
    bool          unexpected_exception;
    FLFailureType failure_type; ///< type of failure that occurred (setup/test/cleanup)
    /// Owned copy of the exception's details. FL_DETAILS points into a per-thread
    /// scratch buffer that the next detail-formatting throw or assertion
    /// overwrites, so the text must be captured here to survive past the catch
    /// block. Because details points at this array, copying a TestResult by value
    /// leaves the copy's details pointing at the source; pass TestResult by
    /// pointer instead.
    char details_buf[FL_MAX_DETAILS_LENGTH];
} TestResult;

static inline void test_result_init(TestResult *tr, char const *reason,
                                    char const *details, char const *file, int line,
                                    FLResultCode rc, bool unexpected,
                                    FLFailureType failure_type) {
    tr->reason = reason;
    if (details != NULL) {
        snprintf(tr->details_buf, sizeof tr->details_buf, "%s", details);
        tr->details = tr->details_buf;
    } else {
        tr->details_buf[0] = '\0';
        tr->details        = NULL;
    }
    tr->file                 = file;
    tr->line                 = line;
    tr->rc                   = rc;
    tr->unexpected_exception = unexpected;
    tr->failure_type         = failure_type;
}

static inline void test_result_clear(TestResult *tr) {
    memset(tr, 0, sizeof *tr);
}

#endif // FAULTLINE_TEST_RESULT_H_
