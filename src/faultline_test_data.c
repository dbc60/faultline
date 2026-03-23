/**
 * @file faultline_test_data.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "arena_dbg.c"
#include "arena.c"
#include "arena_malloc.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "fl_exception_service.c"
#include "flp_exception_service.c"
#include "fault_injector.c"
#include "fla_memory_service.c"
#include "faultline_context.c"
#include "faultline_driver.c"
#include "../third_party/fnv/FNV64.c"
#include "flp_log_service.c"
#include "region.c"
#include "region_node.c"
#include "set.c"
#include "win_timer.c"

// Dummy fla_set_exception_service so the driver can load it via GetProcAddress.
FL_DECL_SPEC FLA_SET_EXCEPTION_SERVICE_FN(fla_set_exception_service) {
    FL_UNUSED(svc);
    FL_UNUSED(size);
}

FL_SPEC_EXPORT FL_EXCEPTION_HANDLER_FN(test_data_handler) {
    FL_UNUSED(ctx);
    if (FL_UNEXPECTED_EXCEPTION(reason)) {
        if (details != NULL) {
            printf("  test_data_handler: Unexpected Exception. %s, %s, @%s: %d\n",
                   reason, details, file, line);
        } else {
            printf("  test_data_handler: Unexpected Exception. %s, @%s: %d\n", reason,
                   file, line);
        }
    }
}

FL_SPEC_EXPORT FL_INITIALIZE(test_data_initialize) {
    faultline_initialize(fctx, ts, arena);
}

// Test wrappers around the driver routines that are exported from the DLL
FL_SPEC_EXPORT FL_IS_VALID(test_data_is_valid) {
    return faultline_is_valid(fctx);
}

FL_SPEC_EXPORT FL_BEGIN(test_data_begin) {
    faultline_begin(fctx, test_suite);
}

FL_SPEC_EXPORT FL_END(test_data_end) {
    faultline_end(fctx);
}

FL_SPEC_EXPORT FL_ADD_FAULT(test_data_add_fault) {
    faultline_add_fault(fctx, index, code, resource, reason, details, file, line);
}

FL_SPEC_EXPORT FL_NEXT(test_data_next) {
    faultline_next(fctx);
}

FL_SPEC_EXPORT FL_HAS_MORE(test_data_has_more) {
    return faultline_has_more(fctx);
}

FL_SPEC_EXPORT FL_GET_CASE_NAME(test_data_get_case_name) {
    return faultline_get_case_name(fctx);
}

FL_SPEC_EXPORT FL_GET_RESULT_CODE(test_data_get_result_code) {
    return faultline_get_result_code(fctx, index);
}

FL_SPEC_EXPORT FL_GET_INDEX(test_data_get_index) {
    return faultline_get_index(fctx);
}

FL_SPEC_EXPORT FL_EXERCISE_TEST(test_data_run_test) {
    faultline_run_test(fctx);
}

FL_SPEC_EXPORT FL_GET_RUN_COUNT(test_data_get_run_count) {
    return faultline_get_run_count(fctx);
}

FL_SPEC_EXPORT FL_GET_PASS_COUNT(test_data_get_pass_count) {
    return faultline_get_pass_count(fctx);
}

FL_SPEC_EXPORT FL_GET_FAIL_COUNT(test_data_get_fail_count) {
    return faultline_get_fail_count(fctx);
}

FL_SPEC_EXPORT FL_GET_SETUP_FAIL_COUNT(test_data_get_setup_fail_count) {
    return faultline_get_setup_fail_count(fctx);
}

FL_SPEC_EXPORT FL_GET_CLEANUP_FAIL_COUNT(test_data_get_cleanup_fail_count) {
    return faultline_get_cleanup_fail_count(fctx);
}

FL_SPEC_EXPORT FL_GET_RESULTS_COUNT(test_data_get_results_count) {
    return faultline_get_results_count(fctx);
}
