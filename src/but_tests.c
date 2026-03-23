/**
 * @file but_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the Basic Unit Test (BUT) library.
 * @version 0.2
 * @date 2026-01-30
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "but_driver.c"
#include "but_result_context.c"
#include "but_test_cases.c"
#include "fl_exception_service.c"
#include "fla_exception_service.c"
#include "fla_log_service.c"

FL_SUITE_BEGIN(driver)
FL_SUITE_ADD_EMBEDDED(load_driver)
FL_SUITE_ADD_EMBEDDED(begin_end)
FL_SUITE_ADD_EMBEDDED(is_valid)
FL_SUITE_ADD_EMBEDDED(next_index)
FL_SUITE_ADD_EMBEDDED(case_name)
FL_SUITE_ADD_EMBEDDED(case_index)
FL_SUITE_ADD_EMBEDDED(test)
FL_SUITE_ADD_EMBEDDED(results)
FL_SUITE_END;
FL_GET_TEST_SUITE("Basic Driver", driver)
