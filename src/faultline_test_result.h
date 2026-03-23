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
#include <faultline/fl_result_codes.h>
#include <faultline/fl_types.h>

#include <stdbool.h> // bool
#include <string.h>  // for memset

typedef struct TestResult {
    char const   *reason;
    char const   *details;
    char const   *file;
    int           line;
    FLResultCode  rc;
    bool          unexpected_exception;
    FLFailureType failure_type; ///< type of failure that occurred (setup/test/cleanup)
} TestResult;

static inline void test_result_init(TestResult *tr, char const *reason,
                                    char const *details, char const *file, int line,
                                    FLResultCode rc, bool unexpected,
                                    FLFailureType failure_type) {
    tr->reason               = reason;
    tr->details              = details;
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
