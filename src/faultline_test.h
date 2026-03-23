#ifndef FAULTLINE_TEST_H_
#define FAULTLINE_TEST_H_

/**
 * @file faultline_test.h
 * @author Douglas Cuthbertson
 * @brief Test Cases for the C Resiliency Yardstick API.
 * @version 0.1
 * @date 2024-12-20
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_context.h> // FLContext
#include <faultline_driver.h>     // FL_IS_VALID, FL_INITIALIZE
#include <faultline/arena.h>      // Arena, new_arena, release_arena

#include <faultline/fl_test.h> // FLTestCase, FLTestSuite

#include <stddef.h>  // size_t
#include <stdbool.h> // bool

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

struct TestDriverData {
    HMODULE                              h;
    Arena                               *arena;
    FLContext                            context;
    FLTestCase                           tc;
    FLTestSuite                         *ts;
    faultline_initialize_fn             *initialize;
    faultline_is_valid_fn               *is_valid;
    faultline_begin_fn                  *begin;
    faultline_end_fn                    *end;
    faultline_add_fault_fn              *add_fault;
    faultline_next_fn                   *next;
    faultline_has_more_fn               *has_more;
    faultline_get_case_name_fn          *get_case_name;
    faultline_get_result_code_fn        *get_result_code;
    faultline_get_index_fn              *get_index;
    faultline_driver_fn                 *driver;
    faultline_get_run_count_fn          *get_run_count;
    faultline_get_pass_count_fn         *get_pass_count;
    faultline_get_fail_count_fn         *get_fail_count;
    faultline_get_setup_fail_count_fn   *get_setup_fail_count;
    faultline_get_cleanup_fail_count_fn *get_cleanup_fail_count;
    faultline_get_results_count_fn      *get_results_count;
};
typedef struct TestDriverData TestDriverData;

extern TestDriverData test_test_case;

#endif // FAULTLINE_TEST_H_
