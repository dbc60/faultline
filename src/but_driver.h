#ifndef BUT_DRIVER_H_
#define BUT_DRIVER_H_

/**
 * @file but_driver.h
 * @author Douglas Cuthbertson
 * @brief context definitions for the Basic Unit Test (BUT) module.
 * @version 0.1
 * @date 2023-12-09
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_exception_types.h> // for FLExceptionReason
#include <faultline/fl_test.h>            // for FLTestSuite
#include <stdbool.h>                      // for bool
#include <stddef.h>                       // for size_t
#include "but_context.h"                  // for BasicContext, BasicResultCode

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Note that bd_is_valid is declared using an experimental form for defining an
 * interface for the test driver. If used correctly, it ensures that implementations of
 * this function and pointers to them are the same type. For example, but_test_cases.h
 * uses pointers to functions that are supposed to be identical to the BUT API. If
 * instead of defining its own pointer types, it uses pointers to the declared function
 * types, then the pointers are guaranteed to be the correct type.
 *
 * Also, the macros can be used to define the function itself; see the definition of
 * bd_is_valid in bd_driver.c, for example.
 */

/**
 * @brief determine if the context has been properly initialized (by bd_begin).
 *
 * @param bctx a test context
 * @return true if the test context is valid and false otherwise.
 */
#define BD_IS_VALID(name) bool name(BasicContext *bctx)
typedef BD_IS_VALID(bd_is_valid_fn);
BD_IS_VALID(bd_is_valid);

#define BD_LOG_ERROR(name)                                                          \
    void name(char const *test_case, FLExceptionReason reason, char const *details, \
              char const *file, int line)
typedef BD_LOG_ERROR(bd_log_error_fn);
BD_LOG_ERROR(bd_log_error);

/**
 * @brief bd_begin assigns a test suite to a test context.
 *
 * @param bctx a test context.
 * @param bts a test suite.
 */
#define BD_BEGIN(name) void name(BasicContext *bctx, struct FLTestSuite *bts)
typedef BD_BEGIN(bd_begin_fn);
BD_BEGIN(bd_begin);

/**
 * @brief bd_end releases the memory resources allocated during testing.
 *
 * @param bctx test context
 */
#define BD_END(name) void name(BasicContext *bctx)
typedef BD_END(bd_end_fn);
BD_END(bd_end);

/**
 * @brief bd_next is part of the iterator interface. If there is another test case in
 * the test suite, the context is updated to hold that test case as the current one. If
 * there are no more test cases, the context is unchanged.
 *
 * @param bctx a test context.
 */
#define BD_NEXT(name) void name(BasicContext *bctx)
typedef BD_NEXT(bd_next_fn);
BD_NEXT(bd_next);

/**
 * @brief bd_has_more is part of the iterator interface. It determines if there is
 * another test case in the test suite,
 *
 * @param cts a test context.
 *
 * @return true if there are more test cases after the current one, and false otherwise.
 */
#define BD_HAS_MORE(name) bool name(BasicContext *bctx)
typedef BD_HAS_MORE(bd_has_more_fn);
BD_HAS_MORE(bd_has_more);

/**
 * @brief bd_get_test_case_name retrieves the name of the current test case.
 *
 * @param bctx a test context.
 * @param bts a test suite.
 *
 * @return the name of the current test case.
 */
#define BD_GET_TEST_CASE_NAME(name) char const *name(BasicContext *bctx)
typedef BD_GET_TEST_CASE_NAME(bd_get_test_case_name_fn);
BD_GET_TEST_CASE_NAME(bd_get_test_case_name);

/**
 * @brief retrieve the index of the current test case.
 *
 * @param bctx a test context.
 * @return the zero-based index of the test case.
 */
#define BD_GET_INDEX(name) size_t name(BasicContext *bctx)
typedef BD_GET_INDEX(bd_get_index_fn);
BD_GET_INDEX(bd_get_index);

/**
 * @brief bd_driver executes the current test case.
 *
 * @param bctx a test context.
 */
#define BD_DRIVER(name) void name(BasicContext *bctx)
typedef BD_DRIVER(bd_driver_fn);
BD_DRIVER(bd_driver);

/**
 * @brief retrieve the number of test cases executed.
 *
 * @param bctx a test context.
 * @return the number of test cases executed.
 */
#define BD_GET_RUN_COUNT(name) size_t name(BasicContext *bctx)
typedef BD_GET_RUN_COUNT(bd_get_run_count_fn);
BD_GET_RUN_COUNT(bd_get_run_count);

/**
 * @brief retrieve the number of tests that passed.
 *
 * @param bctx a test context.
 * @return  the number of tests that passed.
 */
#define BD_GET_PASS_COUNT(name) size_t name(BasicContext *bctx)
typedef BD_GET_PASS_COUNT(bd_get_pass_count_fn);
BD_GET_PASS_COUNT(bd_get_pass_count);

/**
 * @brief retrieve the number of tests that failed.
 *
 * @param bctx a test context
 * @return the number of tests that failed.
 */
#define BD_GET_TEST_FAILURE_COUNT(name) size_t name(BasicContext *bctx)
typedef BD_GET_TEST_FAILURE_COUNT(bd_get_test_failure_count_fn);
BD_GET_TEST_FAILURE_COUNT(bd_get_test_failure_count);

/**
 * @brief retrieve the number of tests where the setup failed.
 *
 * @param bctx a test context
 * @return the number of tests where the setup failed.
 */
#define BD_GET_SETUP_FAILURE_COUNT(name) size_t name(BasicContext *bctx)
typedef BD_GET_SETUP_FAILURE_COUNT(bd_get_setup_failure_count_fn);
BD_GET_SETUP_FAILURE_COUNT(bd_get_setup_failure_count);

/**
 * @brief retrieve the number of tests that failed in the cleanup phase.
 *
 * @param bctx a test context.
 * @return the number of tests that failed in the cleanup phase.
 */
#define BD_GET_CLEANUP_FAILURE_COUNT(name) size_t name(BasicContext *bctx)
typedef BD_GET_CLEANUP_FAILURE_COUNT(bd_get_cleanup_failure_count_fn);
BD_GET_CLEANUP_FAILURE_COUNT(bd_get_cleanup_failure_count);

/**
 * @brief retrieve the number of result contexts.
 *
 * @param bctx a test context.
 * @return the number of result contexts.
 */
#define BD_GET_RESULTS_COUNT(name) size_t name(BasicContext *bctx)
typedef BD_GET_RESULTS_COUNT(bd_get_results_count_fn);
BD_GET_RESULTS_COUNT(bd_get_results_count);

/**
 * @brief retrieve the result code from a result context.
 *
 * @param bctx a test context.
 * @param index the index of a result context.
 * @return a test-result code.
 */
#define BD_GET_RESULT(name) BasicResultCode name(BasicContext *bctx, size_t index)
typedef BD_GET_RESULT(bd_get_result_fn);
BD_GET_RESULT(bd_get_result);

#if defined(__cplusplus)
}
#endif

#endif // BUT_DRIVER_H_
