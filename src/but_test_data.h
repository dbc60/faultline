#ifndef BUT_TEST_DATA_H_
#define BUT_TEST_DATA_H_

/**
 * @file but_test_data.h
 * @author Douglas Cuthbertson
 * @brief Definitions for testing the Basic Unit Test (BUT) library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "but_context.h" // BasicContext

#include <faultline/fl_test.h>              // FLTestSuite
#include <faultline/fl_abbreviated_types.h> // u32
#include <faultline/fl_macros.h>            // FL_DECL_SPEC

#include <stdbool.h> // bool

// GetProcAddress never uses wchar_t, so these are regular C-strings
#define IS_VALID_CTX_STR             "test_data_is_valid"
#define BEGIN_CTX_STR                "test_data_begin"
#define END_CTX_STR                  "test_data_end"
#define NEXT_CTX_STR                 "test_data_next"
#define MORE_CASES_CTX_STR           "test_data_more"
#define GET_CASE_NAME_CTX_STR        "test_data_get_test_case_name"
#define GET_CASE_INDEX_CTX_STR       "test_data_get_index"
#define RUN_CURRENT_CTX_STR          "test_data_run_test"
#define GET_PASS_COUNT_CTX_STR       "test_data_get_pass_count"
#define GET_FAIL_COUNT_CTX_STR       "test_data_get_fail_count"
#define GET_SETUP_FAIL_COUNT_CTX_STR "test_data_get_setup_fail_count"
#define GET_RESULTS_COUNT_CTX_STR    "test_data_get_results_count"
#define GET_RESULT_CTX_STR           "test_data_get_result"

FL_DECL_SPEC BD_IS_VALID(test_data_is_valid);
FL_DECL_SPEC BD_BEGIN(test_data_begin);
FL_DECL_SPEC BD_END(test_data_end);
FL_DECL_SPEC BD_NEXT(test_data_next);
FL_DECL_SPEC BD_HAS_MORE(test_data_more);
FL_DECL_SPEC BD_GET_TEST_CASE_NAME(test_data_get_test_case_name);
FL_DECL_SPEC BD_GET_INDEX(test_data_get_index);
FL_DECL_SPEC BD_DRIVER(test_data_run_test);
FL_DECL_SPEC BD_GET_RUN_COUNT(test_data_get_run_count);
FL_DECL_SPEC BD_GET_PASS_COUNT(test_data_get_pass_count);
FL_DECL_SPEC BD_GET_SETUP_FAILURE_COUNT(test_data_get_fail_count);
FL_DECL_SPEC BD_GET_SETUP_FAILURE_COUNT(test_data_get_setup_fail_count);
FL_DECL_SPEC BD_GET_RESULTS_COUNT(test_data_get_results_count);
FL_DECL_SPEC BD_GET_RESULT(test_data_get_result);

#endif // BUT_TEST_DATA_H_
