/**
 * @file but_test_data.c
 * @author Douglas Cuthbertson
 * @brief Test data for testing the Basic Unit Test (BUT) library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "but_driver.c"
#include "but_result_context.c"
#include "fl_exception_service.c"
#include "flp_exception_service.c"
#include "flp_log_service.c"
#include "fl_threads.c" // mtx_init

// Dummy fla_set_exception_service so set_up_test_driver_data() in but_test_cases.c has
// a function to load.
FL_DECL_SPEC FLA_SET_EXCEPTION_SERVICE_FN(fla_set_exception_service) {
    FL_UNUSED(svc);
    FL_UNUSED(size);
}

FL_SPEC_EXPORT FL_EXCEPTION_HANDLER_FN(test_data_handler) {
    BasicContext *bctx = (BasicContext *)ctx;

    if (FL_UNEXPECTED_EXCEPTION(reason)) {
        FLTestSuite *ts = bctx->ts;
        char        *name;
        if (bctx->index < ts->count) {
            name = ts->test_cases[bctx->index]->name;
        } else {
            name = "No test case";
        }

        if (details != NULL) {
            printf("  test_data_handler: Unexpected Exception. %s: %s, %s, @%s:%d\n",
                   name, reason, details, file, line);
        } else {
            printf("  test_data_handler: Unexpected Exception. %s: %s, @%s:%d\n", name,
                   reason, file, line);
        }
    }
}

// Test wrappers around the driver routines that are exported from the DLL
FL_SPEC_EXPORT BD_IS_VALID(test_data_is_valid) {
    return bd_is_valid(bctx);
}

FL_SPEC_EXPORT BD_BEGIN(test_data_begin) {
    bd_begin(bctx, bts);
}

FL_SPEC_EXPORT BD_END(test_data_end) {
    bd_end(bctx);
}

FL_SPEC_EXPORT BD_NEXT(test_data_next) {
    bd_next(bctx);
}

FL_SPEC_EXPORT BD_HAS_MORE(test_data_more) {
    return bd_has_more(bctx);
}

FL_SPEC_EXPORT BD_GET_TEST_CASE_NAME(test_data_get_test_case_name) {
    return bd_get_test_case_name(bctx);
}

FL_SPEC_EXPORT BD_GET_INDEX(test_data_get_index) {
    return bd_get_index(bctx);
}

FL_SPEC_EXPORT BD_DRIVER(test_data_run_test) {
    bd_driver(bctx);
}

FL_SPEC_EXPORT BD_GET_RUN_COUNT(test_data_get_run_count) {
    return bd_get_run_count(bctx);
}

FL_SPEC_EXPORT BD_GET_PASS_COUNT(test_data_get_pass_count) {
    return bd_get_pass_count(bctx);
}

FL_SPEC_EXPORT BD_GET_SETUP_FAILURE_COUNT(test_data_get_fail_count) {
    return bd_get_test_failure_count(bctx);
}

FL_SPEC_EXPORT BD_GET_SETUP_FAILURE_COUNT(test_data_get_setup_fail_count) {
    return bd_get_setup_failure_count(bctx);
}

FL_SPEC_EXPORT BD_GET_RESULTS_COUNT(test_data_get_results_count) {
    return bd_get_results_count(bctx);
}

FL_SPEC_EXPORT BD_GET_RESULT(test_data_get_result) {
    return bd_get_result(bctx, index);
}
