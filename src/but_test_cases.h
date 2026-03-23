#ifndef BUT_TEST_CASES_H_
#define BUT_TEST_CASES_H_

/**
 * @file but_test_cases.h
 * @author Douglas Cuthbertson
 * @brief Test cases for the Basic Unit Test (BUT) library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "but_driver.h"
#include <faultline/fl_exception_service.h> // fla_set_exception_service
#include <faultline/fl_test.h>              // FLTestCase, FLTestSuite

#include <stddef.h>  // size_t
#include <stdbool.h> // bool

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// Test driver's data
struct TestDriverData {
    HMODULE                        h;
    fla_set_exception_service_fn  *set_service;
    FLExceptionService             exception_service;
    BasicContext                   context;
    FLTestCase                     tc;
    FLTestSuite                   *ts;
    bd_begin_fn                   *begin;
    bd_end_fn                     *end;
    bd_is_valid_fn                *is_valid;
    bd_next_fn                    *next;
    bd_has_more_fn                *more;
    bd_get_test_case_name_fn      *get_test_case_name;
    bd_get_index_fn               *get_index;
    bd_driver_fn                  *test;
    bd_get_pass_count_fn          *get_pass_count;
    bd_get_test_failure_count_fn  *get_fail_count;
    bd_get_setup_failure_count_fn *get_failed_set_up_count;
    bd_get_results_count_fn       *get_results_count;
    bd_get_result_fn              *get_result;
};
typedef struct TestDriverData TestDriverData;

extern TestDriverData load_driver_case;
extern TestDriverData begin_end_case;
extern TestDriverData is_valid_case;
extern TestDriverData next_index_case;
extern TestDriverData test_case_name_case;
extern TestDriverData test_case_index_case;
extern TestDriverData test_case;
extern TestDriverData results_case;

#endif // BUT_TEST_CASES_H_
