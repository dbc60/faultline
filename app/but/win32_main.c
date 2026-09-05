/**
 * @file bd_win32_main.c
 * @author Douglas Cuthbertson
 * @brief The test driver for the Basic Unit Test (BUT) library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

// Must be defined before any headers are included so that fl_log.h and fl_try.h
// select the platform (driver) implementations of the log and exception services.
#ifndef FL_PLATFORM_BUILD
#define FL_PLATFORM_BUILD
#endif

#include <faultline/fl_log.h> // LOG_* (selects flp_ backend via FL_PLATFORM_BUILD)

#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "but_driver.c"
#include "but_result_context.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "fault_injector.c"
#include "fl_exception.c"
#include "fl_threads.c"
#include "flp_log_service.c"
#include "flp_memory_service.c"
#include "flp_fault_memory_service.c"
#include "flp_file_service.c"
#include "flp_stream_service.c"
#include "flp_timer_service.c"
#include "fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"
#include "lock_os.c"
#include "set.c"

#include <stddef.h> // size_t
#include <stdio.h>  // printf, snprintf
#include <stdlib.h> // exit

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // HMODULE

static char const *module = "Basic Test";

/**
 * @brief The exception handler for the BUT test driver.
 *
 * It's basically the same as the default exception handler, but it prints its output to
 * stdout instead of stderr.
 *
 * @param ctx a pointer to the current BUTExceptionContext.
 * @param reason a brief reason for throwing the exception.
 * @param details a (possibly NULL) string providing more details about the exception.
 * @param file the path to the file in which the exception was thrown.
 * @param line the line number on which the exception was thrown.
 */
static FL_EXCEPTION_HANDLER_FN(exception_handler) {
    BasicContext *bctx = (BasicContext *)ctx;

    if (FL_UNEXPECTED_EXCEPTION(reason)) {
        FLTestSuite *ts = bctx->ts;
        char const  *name;
        if (bctx->index < ts->count) {
            name = ts->test_cases[bctx->index]->name;
        } else {
            name = "Unknown";
        }
        bd_log_error(name, reason, details, file, line);
    }
}

static void display_test_case(BasicContext *bctx) {
    char        counter_buf[16]; // 16 bytes should be plenty for a counter.
    char const *test_case_name;
    size_t      idx;

    test_case_name = bd_get_test_case_name(bctx);
    idx            = bd_get_index(bctx);

    snprintf(counter_buf, sizeof counter_buf, "%6zu. ", idx + 1);
    // Display  "(leading text) TestCaseName (end text)"
    printf("%s%s\n", counter_buf, test_case_name);
    LOG_INFO(module, "%s%s", counter_buf, test_case_name);
}

static size_t display_test_results(BasicContext *bctx, FLTestSuite *ts) {
    size_t passed, setup_failures, test_failures, cleanup_failures, count_total_failures;
    char   counter_buf[6]  = {0};
    size_t test_case_count = ts->count;
    int    spaces          = 5;
    int    magnitude       = (int)test_case_count;

    while (spaces > 0 && magnitude > 0) {
        magnitude /= 10;
        if (magnitude > 0) {
            spaces--;
        }
    }

    for (int i = 0; i < spaces; i++) {
        counter_buf[i] = ' ';
    }

    passed           = bd_get_pass_count(bctx);
    size_t run_count = bd_get_run_count(bctx);
    if (passed != run_count) {
        printf("\nPassed: %zu of %zu test cases\n", passed, run_count);
        LOG_INFO(module, "Passed: %zu of %zu test cases", passed, run_count);
    } else {
        if (passed == 2) {
            puts("\nBoth tests passed");
            LOG_INFO(module, "Both tests passed");
        } else if (passed == 1) {
            puts("\nThe test passed");
            LOG_INFO(module, "The test passed");
        } else {
            printf("\nAll %zu tests passed\n", passed);
            LOG_INFO(module, "All %zu tests passed", passed);
        }
    }

    setup_failures       = bd_get_setup_failure_count(bctx);
    test_failures        = bd_get_test_failure_count(bctx);
    cleanup_failures     = bd_get_cleanup_failure_count(bctx);
    count_total_failures = setup_failures + test_failures + cleanup_failures;

    if (count_total_failures > 0) {
        printf("Failures: %zu\n", count_total_failures);
        printf("%sFailed Setups: %zu\n", counter_buf, setup_failures);
        printf("%sFailed Tests: %zu\n", counter_buf, test_failures);
        printf("%sFailed Cleanups: %zu\n", counter_buf, cleanup_failures);
        LOG_INFO(module, "Failures: %zu", count_total_failures);
        LOG_INFO(module, "%sFailed Setups: %zu", counter_buf, setup_failures);
        LOG_INFO(module, "%sFailed Tests: %zu", counter_buf, test_failures);
        LOG_INFO(module, "%sFailed Cleanups: %zu", counter_buf, cleanup_failures);
    }

    return count_total_failures;
}

static size_t exercise_test_suite(BasicContext *bctx, FLTestSuite *ts,
                                  fl_run_case_fn *run_case) {
    bd_begin(bctx, ts, run_case);
    while (bd_has_more(bctx)) {
        display_test_case(bctx);
        FL_TRY {
            bd_driver(bctx);
        }
        FL_END_TRY;
        bd_next(bctx);
    }

    return display_test_results(bctx, ts);
}

/**
 * @brief the entry point for a simple command-line test driver.
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char **argv) {
    int                   i;
    char                 *ts_path;
    HMODULE               test_suite;
    fl_get_test_suite_fn *fl_get_test_suite;
    FLTestSuite          *ts;
    BasicContext          bctx;
    Arena                *arena = new_arena(0, 0);

    // volatile because FL_TRY's setjmp and a throw's longjmp bracket the writes to this
    // counter: a non-volatile local written in between has an indeterminate value after
    // the jump. bd_begin clears the context per suite, so the per-suite counts sum.
    size_t volatile failures = 0;

    if (argc > 1) {
        FaultInjector        fi;
        FLFaultMemoryContext flmctx;
        fault_injector_init(&fi, arena);
        flp_init_fault_memory_context(&flmctx, arena, &fi);

        flp_log_init_custom(LOG_LEVEL_INFO, "but_test.log");
        LOG_INFO(module, "Log out path set");

        // Assume each argument is a path to a test suite
        for (i = 1; i < argc; i++) {
            ts_path    = argv[i];
            test_suite = LoadLibraryA(ts_path);
            if (test_suite == NULL) {
                DWORD lastError = GetLastError();
                printf("Failed to load test suite %s, error = %lu\n", ts_path,
                       lastError);
                LOG_ERROR(module, "Failed to load test suite %s, error = %lu\n", ts_path,
                          lastError);
                failures++;
                continue;
            }

            fla_set_log_service_fn *fla_set_log_service
                = (fla_set_log_service_fn *)GetProcAddress(test_suite,
                                                           FLA_SET_LOG_SERVICE_STR);
            // the log service is optional
            if (fla_set_log_service != NULL) {
                flp_init_log_service(fla_set_log_service);
                LOG_INFO(module, "Logging Connected");
            }

            fla_set_memory_service_fn *fla_set_memory_service
                = (fla_set_memory_service_fn *)
                    GetProcAddress(test_suite, FLA_SET_MEMORY_SERVICE_STR);
            // the memory service is optional
            if (fla_set_memory_service != NULL) {
                flp_init_fault_memory_service(fla_set_memory_service, &flmctx);
            }

            fla_set_file_service_fn *fla_set_file_service
                = (fla_set_file_service_fn *)GetProcAddress(test_suite,
                                                            FLA_SET_FILE_SERVICE_STR);
            // the file service is optional
            if (fla_set_file_service != NULL) {
                flp_init_file_service(fla_set_file_service);
            }

            fla_set_stream_service_fn *fla_set_stream_service
                = (fla_set_stream_service_fn *)
                    GetProcAddress(test_suite, FLA_SET_STREAM_SERVICE_STR);
            // the stream service is optional
            if (fla_set_stream_service != NULL) {
                flp_init_stream_service(fla_set_stream_service);
            }

            fla_set_timer_service_fn *fla_set_timer_service
                = (fla_set_timer_service_fn *)GetProcAddress(test_suite,
                                                             FLA_SET_TIMER_SERVICE_STR);
            // the timer service is optional
            if (fla_set_timer_service != NULL) {
                flp_init_timer_service(fla_set_timer_service);
            }

            fl_get_test_suite
                = (fl_get_test_suite_fn *)GetProcAddress(test_suite,
                                                         "fl_get_test_suite");
            if (fl_get_test_suite == NULL) {
                LOG_ERROR(module, "Test suite %s doesn't export fl_get_test_suite",
                          ts_path);
                failures++;
                continue;
            }

            // FL_GET_TEST_SUITE emits this alongside fl_get_test_suite. Without it
            // there is no way into the suite's cases: this driver does not call them
            // directly.
            fl_run_case_fn *fl_run_case
                = (fl_run_case_fn *)GetProcAddress(test_suite, FL_RUN_CASE_STR);
            if (fl_run_case == NULL) {
                LOG_ERROR(module, "Test suite %s doesn't export fl_run_case", ts_path);
                failures++;
                continue;
            }

            FL_TRY {
                ts = fl_get_test_suite();
                printf("\n%s (%zu): test suite %d of %d\n", ts->name, ts->count, i,
                       argc - 1);
                LOG_INFO(module, "%s (%zu): test suite %d of %d", ts->name, ts->count, i,
                         argc - 1);
                failures += exercise_test_suite(&bctx, ts, fl_run_case);
                if (i < argc - 1) {
                    printf("*******************************************\n");
                }
            }
            FL_CATCH_ALL {
                exception_handler(&bctx, FL_REASON, FL_DETAILS, FL_FILE, FL_LINE);
                failures++;
            }
            FL_END_TRY;

            FreeLibrary(test_suite);
        }

        flp_log_cleanup();
    } else {
        printf("Usage: %s (path to test suite)+\n", argv[0]);
        LOG_ERROR(module, "Usage: %s (path to test suite)+", argv[0]);
        return -1;
    }

    return failures == 0 ? 0 : 1;
}
