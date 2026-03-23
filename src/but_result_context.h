#ifndef BUT_RESULT_CONTEXT_H_
#define BUT_RESULT_CONTEXT_H_

/**
 * @file but_result_context.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-05-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <stddef.h>      // for size_t
#include "but_context.h" // for BasicResultCode, BasicContext

/**
 * @brief The status and exception generated from a test case that did not pass.
 *
 * The ResultContext only exists for tests that ran, but didn't pass. All tests that run
 * and pass don't generate one of these.
 */
struct ResultContext {
    size_t          index;  ///< the index of the test case
    BasicResultCode status; ///< indicate state of test result
    char const     *reason; ///< a string describing the reason for a failure
    char const     *file;
    int             line;
};

/**
 * @brief new_result creates a new result context to capture the reason for a test
 * failure.
 *
 * @param bctx a test context.
 * @param status a test-result code.
 * @param reason a string describing the reason for the test failure.
 * @param file the name of the file in which the test failure occurred.
 * @param line the number of the line on which the test failure occurred.
 */
void new_result(BasicContext *bctx, BasicResultCode status, char const *reason,
                char const *file, int line);

#endif // BUT_RESULT_CONTEXT_H_
