/**
 * @file command_run.c
 * @author Douglas Cuthbertson
 * @brief Implementation of "run" command handler
 * @version 0.1
 * @date 2026-01-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "../../src/command.h"  // RuntimeCommand, has_option, get_string_option
#include "faultline_commands.h" // ExecutionContext, run_cmd
#include "../../src/flp_memory_context.h" // FLMemoryContext (full definition)
#include "../../src/output_junit.h"       // write_junit_xml

#include <faultline/fl_log.h>          // LOG_ERROR, LOG_WARN, LOG_INFO
#include <faultline/fl_try.h>          // FL_TRY, FL_CATCH_ALL, FL_END_TRY
#include <faultline/fl_test.h>         // FLTestSuite, fl_get_test_suite_fn
#include <faultline/fl_context.h>      // FLContext, faultline_initialize, etc.
#include <faultline_driver.h>          // faultline_run_test
#include <faultline_sqlite.h>          // faultline_record_test_run_start, etc.
#include <faultline/fl_types.h>        // FLFailureType, FLTestPhase
#include <faultline/fl_result_codes.h> // FL_PASS, etc.
#include <faultline/fl_exception_service.h> // flp_init_exception_service, fla_set_exception_service_fn
#include <faultline/fl_log_types.h> // fla_set_log_service_fn, FLA_SET_LOG_SERVICE_STR
#include <faultline/fl_memory_service.h> // fla_set_memory_service_fn, FLA_SET_MEMORY_SERVICE_STR
#include <flp_log_service.h>             // flp_init_log_service
#include <flp_memory_service.h>          // flp_init_memory_service

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h> // LoadLibraryA, GetProcAddress, FreeLibrary, HMODULE

#include <stdio.h>
#include <string.h>

static char const *failure_type_to_string(FLFailureType type) {
    switch (type) {
    case FL_FAILURE_NONE:
        return "No Failure";
    case FL_SETUP_FAILURE:
        return "Setup";
    case FL_TEST_FAILURE:
        return "Test";
    case FL_CLEANUP_FAILURE:
        return "Cleanup";
    case FL_LEAK_FAILURE:
        return "Leak";
    case FL_INVALID_FREE_FAILURE:
        return "Invalid Free";
    default:
        return "Unknown Failure Type";
    }
}

static char const *test_phase_to_string(FLTestPhase phase) {
    switch (phase) {
    case FL_DISCOVERY_PHASE:
        return "Discovery";
    case FL_INJECTION_PHASE:
        return "Injection";
    default:
        return "Unknown Test Phase";
    }
}

static void display_test_case(FLContext *fctx) {
    char const *test_case_name;
    size_t      idx;

    test_case_name = faultline_get_case_name(fctx);
    idx            = faultline_get_index(fctx);

    if (test_case_name == NULL) {
        LOG_WARN("Faultline Display", "test case name is NULL");
    }
    LOG_INFO(fctx->ts->name, "%zu. %s", idx + 1, test_case_name);
}

static void display_detailed_test_results(FLContext *fctx, FLTestSuite *ts) {
    size_t results_count = faultline_get_results_count(fctx);
    size_t failed_tests  = 0;

    LOG_INFO(fctx->ts->name, "=====================");
    LOG_INFO(fctx->ts->name, "Detailed Test Results");
    LOG_INFO(fctx->ts->name, "=====================");

    for (size_t i = 0; i < results_count; i++) {
        FLTestSummary *summary   = faultline_test_summary_buffer_get(&fctx->results, i);
        char const    *test_name = (summary->index < ts->count)
                                       ? ts->test_cases[summary->index]->name
                                       : "Unknown Test Case";

        if (summary->code != FL_PASS) {
            failed_tests++;
            LOG_INFO(fctx->ts->name, "Test %u: %s - %s", summary->index + 1, test_name,
                     faultline_result_code_to_string(summary->code));
            LOG_INFO(fctx->ts->name, "  Phase: %s",
                     test_phase_to_string(summary->failure_phase));
            LOG_INFO(fctx->ts->name, "  Type: %s",
                     failure_type_to_string(summary->failure_type));
            if (summary->reason != NULL) {
                LOG_INFO(fctx->ts->name, "  Reason: %s", summary->reason);
            }
            LOG_INFO(fctx->ts->name, "  Duration: %.3f seconds",
                     summary->elapsed_seconds);
            if (summary->faults_exercised > 0) {
                LOG_INFO(fctx->ts->name, "  Faults Exercised: %lld",
                         summary->faults_exercised);
            }
            if (summary->discovery_failures > 0 || summary->injection_failures > 0) {
                LOG_INFO(fctx->ts->name,
                         "  Discovery Failures: %u, Injection Failures: %u",
                         summary->discovery_failures, summary->injection_failures);
            }
            LOG_INFO(fctx->ts->name, "");
        }
    }

    if (failed_tests == 0) {
        LOG_INFO(fctx->ts->name, "No test failures to display.");
    }
}

static void display_test_results(FLContext *fctx, FLTestSuite *ts) {
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

    passed           = faultline_get_pass_count(fctx);
    size_t run_count = faultline_get_run_count(fctx);

    LOG_INFO(fctx->ts->name, "============");
    LOG_INFO(fctx->ts->name, "Test Summary");
    LOG_INFO(fctx->ts->name, "============");

    if (passed != run_count) {
        LOG_INFO(fctx->ts->name, "Passed: %zu of %zu test cases (%.1f%%)", passed,
                 run_count, (passed * 100.0) / run_count);
    } else {
        if (passed == 2) {
            LOG_INFO(fctx->ts->name, "Both tests passed (100%)");
        } else if (passed == 1) {
            LOG_INFO(fctx->ts->name, "The test passed (100%)");
        } else {
            LOG_INFO(fctx->ts->name, "All %zu tests passed (100%%)", passed);
        }
    }

    setup_failures       = faultline_get_setup_fail_count(fctx);
    test_failures        = faultline_get_fail_count(fctx);
    cleanup_failures     = faultline_get_cleanup_fail_count(fctx);
    count_total_failures = setup_failures + test_failures + cleanup_failures;

    if (count_total_failures > 0) {
        LOG_INFO(fctx->ts->name, "Total Failures: %zu", count_total_failures);
        LOG_INFO(fctx->ts->name, "%sFailed Setups: %zu", counter_buf, setup_failures);
        LOG_INFO(fctx->ts->name, "%sFailed Tests: %zu", counter_buf, test_failures);
        LOG_INFO(fctx->ts->name, "%sFailed Cleanups: %zu", counter_buf,
                 cleanup_failures);

        display_detailed_test_results(fctx, ts);
    }

    size_t results_count          = faultline_get_results_count(fctx);
    i64    total_faults_exercised = 0;
    double total_discovery_time   = 0.0;
    double total_injection_time   = 0.0;
    double total_elapsed_time     = 0.0;

    for (size_t i = 0; i < results_count; i++) {
        FLTestSummary *summary = faultline_test_summary_buffer_get(&fctx->results, i);
        total_faults_exercised += summary->faults_exercised;
        total_discovery_time += summary->discovery_time;
        total_injection_time += summary->injection_time;
        total_elapsed_time += summary->elapsed_seconds;
    }

    LOG_INFO(fctx->ts->name, "Tests Run: %zu", run_count);
    LOG_INFO(fctx->ts->name, "Test Results Recorded: %u", results_count);

    if (total_faults_exercised > 0) {
        LOG_INFO(fctx->ts->name, "Fault Site Coverage: %lld fault sites exercised",
                 total_faults_exercised);
        if (run_count > 0) {
            LOG_INFO(fctx->ts->name, "Average Faults per Test: %.1f",
                     (double)total_faults_exercised / run_count);
        }
    } else {
        LOG_INFO(fctx->ts->name, "Fault Site Coverage: No fault sites exercised");
    }

    bool has_phase_timing
        = (total_discovery_time >= 0.001 || total_injection_time >= 0.001);
    bool has_elapsed_timing = (total_elapsed_time >= 0.001);
    bool has_any_results    = (results_count > 0);

    if (has_phase_timing) {
        LOG_INFO(fctx->ts->name, "Timing Breakdown:");
        LOG_INFO(fctx->ts->name, "  Discovery Phase: %.3f seconds",
                 total_discovery_time);
        LOG_INFO(fctx->ts->name, "  Injection Phase: %.3f seconds",
                 total_injection_time);
        LOG_INFO(fctx->ts->name, "  Total Runtime: %.3f seconds",
                 total_discovery_time + total_injection_time);
        if (run_count > 0) {
            LOG_INFO(fctx->ts->name, "  Average per Test: %.3f seconds",
                     (total_discovery_time + total_injection_time) / run_count);
        }
    } else if (has_elapsed_timing) {
        LOG_INFO(fctx->ts->name, "Timing Summary:");
        LOG_INFO(fctx->ts->name,
                 "  Total Runtime: %.3f seconds (phase breakdown not available)",
                 total_elapsed_time);
        if (run_count > 0) {
            LOG_INFO(fctx->ts->name, "  Average per Test: %.3f seconds",
                     total_elapsed_time / run_count);
        }
    } else if (has_any_results) {
        LOG_INFO(fctx->ts->name, "Timing Summary:");
        LOG_INFO(fctx->ts->name, "  Tests completed in less than 1ms");
    } else {
        LOG_INFO(fctx->ts->name, "Timing: No test results available");
    }
}

static void exercise_test_suite(FLContext *fctx, FLTestSuite *ts, sqlite3 *db,
                                JUnitXML *junit) {
    int run_id = -1;

    if (db != NULL) {
        run_id = faultline_record_test_run_start(db, ts->name, fctx->run_start_time);
    }

    faultline_begin(fctx, ts);
    if (!faultline_has_more(fctx)) {
        LOG_WARN("Faultline", "suite %s has no test cases", ts->name);
    }
    while (faultline_has_more(fctx)) {
        display_test_case(fctx);
        faultline_run_test(fctx); // exceptions propagate naturally - no FL_TRY needed
        faultline_next(fctx);
    }

    display_test_results(fctx, ts);
    junit_write(junit, fctx);

    if (db != NULL && run_id > 0) {
        faultline_record_test_run_complete(db, run_id, fctx);
        size_t n = faultline_get_results_count(fctx);
        for (size_t i = 0; i < n; i++) {
            FLTestSummary *summary
                = faultline_test_summary_buffer_get(&fctx->results, i);
            char const *test_name = (summary->index < ts->count)
                                        ? ts->test_cases[summary->index]->name
                                        : "Null Test Case Name";
            faultline_record_test_summary(db, run_id, summary, test_name);
        }
    }

    faultline_end(fctx);
}

/**
 * @brief Handler for "run" command
 *
 * Executes one or more test suites with optional database recording.
 * Positional arguments: test_suite_paths (one or more DLL paths)
 */
COMMAND_HANDLER(run_cmd) {
    ExecutionContext *ectx             = (ExecutionContext *)cmd;
    FLContext        *fctx             = ectx->fctx;
    int               test_suite_count = cmd->argc;
    int               loaded           = 0;
    static char const module[]         = "Faultline Run";

    if (test_suite_count == 0) {
        LOG_ERROR(module, "No test suite paths provided");
        return;
    }

    JUnitXML junit = {0};
    if (ectx->junit_xml_path) {
        junit_init(&junit, ectx->arena, ectx->junit_xml_path);
    }
    junit_begin(&junit);
    FL_TRY {
        for (int i = 0; i < test_suite_count; i++) {
            char const *dll_path   = cmd->args[i];
            HMODULE     test_suite = LoadLibraryA(dll_path);
            if (test_suite == NULL) {
                LOG_ERROR(module, "Failed to load \"%s\", error=%lu", dll_path,
                          GetLastError());
                continue;
            }

            // Log service (optional)
            fla_set_log_service_fn *fla_set_log
                = (fla_set_log_service_fn *)GetProcAddress(test_suite,
                                                           FLA_SET_LOG_SERVICE_STR);
            if (fla_set_log != NULL) {
                flp_init_log_service(fla_set_log);
            }

            // Exception service (required)
            fla_set_exception_service_fn *fla_set_exc = (fla_set_exception_service_fn *)
                GetProcAddress(test_suite, FLA_SET_EXCEPTION_SERVICE_STR);
            if (fla_set_exc == NULL) {
                LOG_ERROR(module, "\"%s\" has no exception service - skipping",
                          dll_path);
                FreeLibrary(test_suite);
                continue;
            }
            flp_init_exception_service(fla_set_exc);

            // Memory service (optional)
            fla_set_memory_service_fn *fla_set_mem = (fla_set_memory_service_fn *)
                GetProcAddress(test_suite, FLA_SET_MEMORY_SERVICE_STR);
            if (fla_set_mem != NULL) {
                flp_init_memory_service(fla_set_mem, ectx->mem_ctx);
            }

            // Run tests
            fl_get_test_suite_fn *fl_get_test_suite
                = (fl_get_test_suite_fn *)GetProcAddress(test_suite,
                                                         "fl_get_test_suite");
            if (fl_get_test_suite != NULL) {
                FL_TRY {
                    FLTestSuite *ts = fl_get_test_suite();
                    faultline_initialize(fctx, ts, ectx->arena);
                    fctx->injector
                        = ectx->mem_ctx
                              ->fi; // restore after memset in faultline_initialize
                    exercise_test_suite(fctx, ts, ectx->db, &junit);
                }
                FL_CATCH_ALL {
                    LOG_ERROR(module, "\"%s\": unhandled exception: %s (%s:%d)",
                              dll_path, FL_REASON, FL_FILE, FL_LINE);
                }
                FL_END_TRY;
                loaded++;
            } else {
                LOG_ERROR(module, "\"%s\" does not export fl_get_test_suite", dll_path);
            }

            FreeLibrary(test_suite);
        }
    }
    FL_FINALLY {
        junit_end(&junit);
    }
    FL_END_TRY;
    LOG_INFO(module, "Exercised %d of %d test suite(s).", loaded, test_suite_count);
}
