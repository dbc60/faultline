/**
 * @file but_result_context.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-05-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "but_result_context.h" // ResultContext
#include <stddef.h>             // for size_t
#include <stdlib.h>             // for realloc
#include <string.h>             // for memset
#include "but_context.h"        // for ResultContext, BasicContext, BasicResultCode

/**
 * @brief increase the capacity of the results array in the context's environment.
 *
 * @param bctx
 */
static void grow_capacity(BasicContext *bctx) {
    ResultContext *new_results;
    size_t         new_capacity, count;
    size_t const   increment = 10;

    // Calculate a new capacity up to the number of test cases in the test suite.
    if (bctx->results_capacity + increment > bctx->test_case_count) {
        new_capacity = bctx->test_case_count;
        count        = bctx->test_case_count - bctx->results_capacity;
    } else {
        new_capacity = bctx->results_capacity + increment;
        count        = increment;
    }

    new_results = realloc(bctx->results, new_capacity * sizeof(ResultContext));
    if (new_results) {
        // Initialize new memory block
        memset(&new_results[bctx->results_capacity], 0, count * sizeof(ResultContext));
        bctx->results_capacity = new_capacity;
        bctx->results          = new_results;
    }
}

// create a new result context to capture the reason for a test failure.
void new_result(BasicContext *bctx, BasicResultCode status, char const *reason,
                char const *file, int line) {
    if (bctx->results_count == bctx->results_capacity) {
        grow_capacity(bctx);
    }

    if (bctx->results_count < bctx->results_capacity) {
        bctx->results[bctx->results_count].index  = bctx->index;
        bctx->results[bctx->results_count].status = status;
        bctx->results[bctx->results_count].reason = reason;
        bctx->results[bctx->results_count].file   = file;
        bctx->results[bctx->results_count].line   = line;
        bctx->results_count++;
    }
}
