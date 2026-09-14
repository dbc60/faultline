/**
 * @file faultline_cli_commands_run.c
 * @author Douglas Cuthbertson
 * @brief The "run" command: load test suites, run their cases, and record each run.
 * @version 0.1
 * @date 2026-01-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit: faultline_cli_commands.c #includes it after the
 * helpers it uses.
 */
#include <faultline/flp_fault_memory_context.h> // FLFaultMemoryContext (full definition)
#include "output_junit.h"                       // write_junit_xml

#include <faultline/fl_log.h>  // LOG_ERROR, LOG_WARN, LOG_INFO
#include <faultline/fl_try.h>  // FL_TRY, FL_CATCH_ALL, FL_END_TRY
#include <faultline/fl_test.h> // FLTestSuite, fl_get_test_suite_fn
#include <faultline/fl_abi.h> // FLAbiInfo, fl_abi_check, fla_get_abi_fn, FLA_GET_ABI_STR
#include <faultline/fl_context.h>      // FLContext, faultline_initialize, etc.
#include <faultline_driver.h>          // faultline_run_test
#include <faultline_dal.h>             // faultline_run_record
#include <faultline/fl_types.h>        // FLFailureType, FLTestPhase
#include <faultline/fl_result_codes.h> // FL_PASS, etc.
#if defined(FL_PLATFORM_BUILD)
// Monolithic driver: the platform's service implementations live in this binary, so
// test-suite loading and injection are direct Win32 + flp_init_* calls.
#include <faultline/fl_exception.h>   // FL_REASON, FL_FILE, FL_LINE
#include <faultline/fl_log_service.h> // fla_set_log_service_fn, FLA_SET_LOG_SERVICE_STR
#include <faultline/fl_memory_service.h> // fla_set_memory_service_fn, FLA_SET_MEMORY_SERVICE_STR
#include <faultline/fl_timer_service.h> // fla_set_timer_service_fn, FLA_SET_TIMER_SERVICE_STR
#include <faultline/fl_file_service.h> // fla_set_file_service_fn, FLA_SET_FILE_SERVICE_STR
#include <faultline/fl_stream_service.h> // fla_set_stream_service_fn, FLA_SET_STREAM_SERVICE_STR
#include <flp_log_service.h>             // flp_init_log_service
#include <flp_memory_service.h>          // flp_init_fault_memory_service
#include <flp_timer_service.h>           // flp_init_timer_service
#include <flp_file_service.h>            // flp_init_file_service
#include <flp_stream_service.h>          // flp_init_stream_service

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h> // LoadLibraryA, GetProcAddress, FreeLibrary, HMODULE
#else
// Split core: all module mechanics route through the platform API the host handed to
// faultline_app_main(); no OS symbol is named here.
#include <platform_api.h> // FLPlatformAPI, FLModule
#endif

#include <stdbool.h> // bool, true, false
#include <stdint.h>  // uintptr_t
#include <stdio.h>
#include <string.h>

#define COMMAND_MODULE_NAME "Faultline Run"

// ---------------------------------------------------------------------------
// The command
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_suites);

static WBFlag run_flags[] = {
    {.name      = "junit-xml",
     .shorthand = 0,
     .type      = WB_FLAG_STRING,
     .desc      = "Also write JUnit XML to this file, e.g. results/junit.xml",
     .def       = {.s = NULL}},
};

static WBCommand run_cmd = {
    .usage      = "run <suite.dll>...",
    .short_desc = "Run test suites and record each run",
    .long_desc  = "Loads each suite DLL, runs its cases with fault injection, and\n"
                  "records the run in the results database unless --no-db is given.\n"
                  "A suite that cannot be loaded is reported and skipped.",
    .example    = "  run arena_tests.dll buffer_tests.dll\n"
                  "  run --junit-xml results/junit.xml arena_tests.dll",
    .flags      = run_flags,
    .flag_count = FL_ARRAY_COUNT(run_flags),
    .min_args   = 1,
    .max_args   = WB_ARGS_ANY,
    .run        = run_suites,
};

static void faultline_cli_add_run(WBCommand *root) {
    wb_command_add(root, &run_cmd);
}

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
            LOG_INFO(fctx->ts->name, "Both tests passed (100%%)");
        } else if (passed == 1) {
            LOG_INFO(fctx->ts->name, "The test passed (100%%)");
        } else {
            LOG_INFO(fctx->ts->name, "All %zu tests passed (100%%)", passed);
        }
    }

    size_t passed_with_leaks = faultline_get_pass_with_leaks_count(fctx);
    if (passed_with_leaks > 0) {
        LOG_INFO(fctx->ts->name, "Passed with leaks: %zu of %zu test cases",
                 passed_with_leaks, run_count);
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
    LOG_INFO(fctx->ts->name, "Test Results Recorded: %zu", results_count);

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
    // faultline_begin stamps fctx->run_start_time and faultline_end clears it
    // after each suite, so the stamp must be taken before the run record is
    // written.
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

    // The run is written only after every case has finished, so a suite that crashes
    // records nothing. A failed write is logged, and the remaining suites still run.
    if (db != NULL) {
        FL_TRY {
            faultline_run_record(db, fctx);
        }
        FL_CATCH_ALL {
            char const *details = (FL_DETAILS != NULL) ? FL_DETAILS : "";
            LOG_ERROR("Faultline", "Failed to record suite %s: %s: %s", ts->name,
                      FL_REASON, details);
            fprintf(stderr, "Error: failed to record suite %s: %s: %s\n", ts->name,
                    FL_REASON, details);
        }
        FL_END_TRY;
    }

    faultline_end(fctx);
}

// -- Suite module mechanics -----------------------------------------------------------
// The run command loads suite modules, injects the platform's services, resolves the
// test suite's entry point, and unloads. The monolithic driver (FL_PLATFORM_BUILD) holds
// the platform symbols itself, so the mechanics are direct OS calls plus the flp_init_*
// injection dance; the split core routes them through session->platform.

#if defined(FL_PLATFORM_BUILD)

typedef HMODULE SuiteHandle;

static SuiteHandle suite_load(FLCliSession *session, char const *path) {
    static char const module[] = COMMAND_MODULE_NAME;
    (void)session;

    HMODULE m = LoadLibraryA(path);
    if (m == NULL) {
        LOG_ERROR(module, "Failed to load \"%s\", error=%lu", path, GetLastError());
    }
    return m;
}

// Inject the driver's services through the fla_set_*_service symbols the test suite
// exports. Every service is optional: a suite that exports no fla_set_ for one keeps
// its own. The memory service injected here is the FAULT-INJECTING binding.
static void suite_inject(FLCliSession *session, SuiteHandle suite) {
    // Log service (optional)
    fla_set_log_service_fn *fla_set_log
        = (fla_set_log_service_fn *)GetProcAddress(suite, FLA_SET_LOG_SERVICE_STR);
    if (fla_set_log != NULL) {
        flp_init_log_service(fla_set_log);
    }

    // Memory service (optional)
    fla_set_memory_service_fn *fla_set_mem
        = (fla_set_memory_service_fn *)GetProcAddress(suite, FLA_SET_MEMORY_SERVICE_STR);
    if (fla_set_mem != NULL) {
        flp_init_fault_memory_service(fla_set_mem, session->mem_ctx);
    }

    // Timer service (optional)
    fla_set_timer_service_fn *fla_set_timer
        = (fla_set_timer_service_fn *)GetProcAddress(suite, FLA_SET_TIMER_SERVICE_STR);
    if (fla_set_timer != NULL) {
        flp_init_timer_service(fla_set_timer);
    }

    // File service (optional)
    fla_set_file_service_fn *fla_set_file
        = (fla_set_file_service_fn *)GetProcAddress(suite, FLA_SET_FILE_SERVICE_STR);
    if (fla_set_file != NULL) {
        flp_init_file_service(fla_set_file);
    }

    // Stream service (optional)
    fla_set_stream_service_fn *fla_set_stream
        = (fla_set_stream_service_fn *)GetProcAddress(suite, FLA_SET_STREAM_SERVICE_STR);
    if (fla_set_stream != NULL) {
        flp_init_stream_service(fla_set_stream);
    }
}

static void *suite_symbol(FLCliSession *session, SuiteHandle suite, char const *symbol) {
    (void)session;
    // The uintptr_t intermediate performs the function-to-data pointer conversion that
    // FARPROC requires (C4152 under /W4 otherwise).
    return (void *)(uintptr_t)GetProcAddress(suite, symbol);
}

static void suite_unload(FLCliSession *session, SuiteHandle suite) {
    (void)session;
    FreeLibrary(suite);
}

static FaultInjector *suite_injector(FLCliSession *session) {
    return session->mem_ctx->fi;
}

#else // split core: route through the platform API

typedef FLModule *SuiteHandle;

static SuiteHandle suite_load(FLCliSession *session, char const *path) {
    return session->platform->load_module(path);
}

static void suite_inject(FLCliSession *session, SuiteHandle suite) {
    session->platform->inject_services(suite);
}

static void *suite_symbol(FLCliSession *session, SuiteHandle suite, char const *symbol) {
    return session->platform->resolve_symbol(suite, symbol);
}

static void suite_unload(FLCliSession *session, SuiteHandle suite) {
    session->platform->unload_module(suite);
}

static FaultInjector *suite_injector(FLCliSession *session) {
    return session->platform->injector;
}

#endif // FL_PLATFORM_BUILD

// Compare a suite's build identity with this driver's. A suite built before the
// descriptor existed does not export it; that is reported and allowed through,
// since refusing it would break every module built against an older contract.
static bool suite_abi_ok(FLCliSession *session, SuiteHandle suite, char const *dll_path) {
    static char const module[] = COMMAND_MODULE_NAME;

    fla_get_abi_fn *get_abi
        = (fla_get_abi_fn *)(uintptr_t)suite_symbol(session, suite, FLA_GET_ABI_STR);
    if (get_abi == NULL) {
        LOG_WARN(module,
                 "\"%s\" reports no build identity; running it unchecked. Rebuild it "
                 "to have the driver verify compatibility.",
                 dll_path);
        return true;
    }

    FLAbiInfo host = {0};
    FLAbiInfo mod  = {0};
    fl_fill_abi_info(&host);
    get_abi(&mod);

    FLAbiVerdict verdict = fl_abi_check(&host, &mod);
    if (verdict == FL_ABI_OK) {
        return true;
    }

    LOG_ERROR(module, "\"%s\" is incompatible with this driver: %s - skipping", dll_path,
              fl_abi_verdict_str(verdict));
    LOG_ERROR(module, "  suite:  %s %u, %s, backend %s, compatibility v%u, v%u.%u.%u",
              fl_abi_compiler_str(mod.compiler_id), mod.compiler_version,
              fl_abi_crt_str(mod.crt_id), fl_abi_backend_str(mod.backend),
              mod.compatibility_version, mod.version_major, mod.version_minor,
              mod.version_patch);
    LOG_ERROR(module, "  driver: %s %u, %s, backend %s, compatibility v%u, v%u.%u.%u",
              fl_abi_compiler_str(host.compiler_id), host.compiler_version,
              fl_abi_crt_str(host.crt_id), fl_abi_backend_str(host.backend),
              host.compatibility_version, host.version_major, host.version_minor,
              host.version_patch);
    LOG_ERROR(module, "  rebuild the suite with the toolchain that built the driver");
    return false;
}

/**
 * @brief Handler for "run" command
 *
 * Executes one or more test suites with optional database recording.
 * Positional arguments: test_suite_paths (one or more DLL paths)
 */
static WB_RUN_FN(run_suites) {
    FLCliSession     *session          = (FLCliSession *)user;
    FLContext        *fctx             = session->fctx;
    int               test_suite_count = argc;
    int               loaded           = 0;
    static char const module[]         = COMMAND_MODULE_NAME;
    FL_UNUSED(io);

    // Without a database the suites still run, and nothing is recorded.
    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_OPTIONAL);

    char const *junit_path = wb_flag_string(cmd, "junit-xml");
    JUnitXML    junit      = {0};
    if (junit_path != NULL) {
        junit_init(&junit, session->arena, junit_path);
    }
    junit_begin(&junit);
    FL_TRY {
        for (int i = 0; i < test_suite_count; i++) {
            char const *dll_path   = argv[i];
            SuiteHandle test_suite = suite_load(session, dll_path);
            if (test_suite == NULL) {
                LOG_ERROR(module, "Failed to load \"%s\"", dll_path);
                continue;
            }

            // Check build identity before running anything. Every struct that crosses
            // this boundary -- the service structs installed below, the FLTestSuite
            // read back, the FLCaseOutcome a case fills in -- has to be laid out the
            // same way by both images, and two toolchains that disagree produce an
            // access violation with no context. Skipping one suite keeps the rest of
            // the run alive.
            if (!suite_abi_ok(session, test_suite, dll_path)) {
                suite_unload(session, test_suite);
                continue;
            }

            suite_inject(session, test_suite);

            // Run tests
            fl_get_test_suite_fn *fl_get_test_suite
                = (fl_get_test_suite_fn *)(uintptr_t)suite_symbol(session, test_suite,
                                                                  FL_GET_TEST_SUITE_STR);
            fl_run_case_fn *fl_run_case
                = (fl_run_case_fn *)(uintptr_t)suite_symbol(session, test_suite,
                                                            FL_RUN_CASE_STR);
            if (fl_get_test_suite == NULL) {
                LOG_ERROR(module, "\"%s\" does not export fl_get_test_suite", dll_path);
            } else if (fl_run_case == NULL) {
                // FL_GET_TEST_SUITE emits both exports together, so a suite with one
                // and not the other was built against an older framework and cannot
                // run its cases.
                LOG_ERROR(module, "\"%s\" does not export fl_run_case - skipping",
                          dll_path);
            } else {
                FL_TRY {
                    FLTestSuite *ts = fl_get_test_suite();
                    faultline_initialize(fctx, ts, session->arena);
                    // Restore what the memset in faultline_initialize cleared.
                    fctx->injector = suite_injector(session);
                    fctx->run_case = fl_run_case;
                    exercise_test_suite(fctx, ts, db, &junit);
                }
                FL_CATCH_ALL {
                    LOG_ERROR(module, "\"%s\": unhandled exception: %s (%s:%d)",
                              dll_path, FL_REASON, FL_FILE, FL_LINE);
                }
                FL_END_TRY;
                loaded++;
            }

            suite_unload(session, test_suite);
        }
    }
    FL_FINALLY {
        junit_end(&junit);
    }
    FL_END_TRY;
    LOG_INFO(module, "Exercised %d of %d test suite(s).", loaded, test_suite_count);
    return WB_CLI_OK;
}
