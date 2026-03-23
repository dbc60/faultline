#ifndef FAULTLINE_TEST_DATA_H_
#define FAULTLINE_TEST_DATA_H_

/**
 * @file faultline_test_data.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-12-31
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
// GetProcAddress never uses wchar_t, so these are regular C-strings
#define INITIALIZE_CTX_STR             "test_data_initialize"
#define IS_VALID_CTX_STR               "test_data_is_valid"
#define BEGIN_CTX_STR                  "test_data_begin"
#define END_CTX_STR                    "test_data_end"
#define ADD_FAULT_CTX_STR              "test_data_add_fault"
#define NEXT_CTX_STR                   "test_data_next"
#define HAS_MORE_CASES_CTX_STR         "test_data_has_more"
#define GET_CASE_NAME_CTX_STR          "test_data_get_case_name"
#define GET_CASE_INDEX_CTX_STR         "test_data_get_index"
#define GET_DRIVER_CTX_STR             "test_data_run_test"
#define GET_RUN_COUNT_CTX_STR          "test_data_get_run_count"
#define GET_PASS_COUNT_CTX_STR         "test_data_get_pass_count"
#define GET_FAIL_COUNT_CTX_STR         "test_data_get_fail_count"
#define GET_SETUP_FAIL_COUNT_CTX_STR   "test_data_get_setup_fail_count"
#define GET_CLEANUP_FAIL_COUNT_CTX_STR "test_data_get_cleanup_fail_count"
#define GET_RESULTS_COUNT_CTX_STR      "test_data_get_results_count"
#define GET_RESULT_CODE_CTX_STR        "test_data_get_result_code"
#define SET_CONTEXT                    "but_set_exception_context"
#define GET_CONTEXT                    "but_get_exception_context"

#endif // FAULTLINE_TEST_DATA_H_
