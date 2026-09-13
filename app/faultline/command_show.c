/**
 * @file command_show.c
 * @author Douglas Cuthbertson
 * @brief Implementation of "show" subcommand handlers
 * @version 0.1
 * @date 2026-01-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "faultline_commands.h"
#include <faultline_dal.h>              // faultline_run_list, faultline_failure_list, ...
#include <faultline/fl_result_codes.h> // faultline_result_code_to_string, FL_PASS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    RUN_SUITE_WIDTH = 20, // characters in the Suite column of "show results"
    ELLIPSIS_LENGTH = 3,  // characters in the "..." that ends a shortened suite name
    LOCATION_WIDTH  = 40  // characters in the location column of a failure or result
};

// Where the row printers write, and how many rows they have written.
typedef struct ShowPrinter {
    FILE *out;
    int   count;
} ShowPrinter;

// The file name at the end of a path.
static char const *show_base_name(char const *path) {
    char const *name = strrchr(path, '\\');
    if (name == NULL) {
        name = strrchr(path, '/');
    }
    return (name != NULL) ? name + 1 : path;
}

// A column that may be NULL, with a fallback to print in its place.
static char const *show_text_or(char const *text, char const *fallback) {
    return (text != NULL) ? text : fallback;
}

// ---------------------------------------------------------------------------------------
// Reports
//
// Each report prints its heading, then hands a row printer to a DAL read. A row's
// strings are valid only while its printer runs, so the printers print them and keep
// nothing.
// ---------------------------------------------------------------------------------------

// One run in "show results".
static bool show_run_row(FLRunRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    // A suite name longer than its column is cut short and ends in "...".
    char suite[RUN_SUITE_WIDTH + 1];
    if (strlen(row->suite_name) > (size_t)RUN_SUITE_WIDTH) {
        snprintf(suite, sizeof suite, "%.*s...", RUN_SUITE_WIDTH - ELLIPSIS_LENGTH,
                 row->suite_name);
    } else {
        snprintf(suite, sizeof suite, "%s", row->suite_name);
    }

    // %.16s keeps the date, hours, and minutes of the start time.
    fprintf(printer->out,
            "%-4lld %-20s %-19.16s %-6lld %-7lld %-4lld/%-4lld %-8.3f %5.1f%%\n",
            (long long)row->id, suite, row->started_at, (long long)row->case_count,
            (long long)row->fault_sites, (long long)row->passed,
            (long long)row->cases_run, row->elapsed_seconds, row->pass_rate * 100.0);
    printer->count++;
    return true;
}

static void show_runs(sqlite3 *db, char const *suite_name, int limit) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    fprintf(printer.out, "\n=== Recent Test Runs ===\n");
    if (suite_name != NULL) {
        fprintf(printer.out, "Suite: %s\n", suite_name);
    }
    fprintf(printer.out, "%-4s %-20s %-19s %-6s %-7s %-4s/%-4s %-8s %-6s\n", "ID", "Suite",
            "Timestamp", "Cases", "Faults", "Pass", "Run", "Time(s)", "Rate%");
    fprintf(printer.out, "%-4s %-20s %-19s %-6s %-7s %-9s %-8s %-6s\n", "----",
            "--------------------", "-------------------", "------", "-------",
            "---------", "--------", "------");

    faultline_run_list(db, suite_name, limit, show_run_row, &printer);
}

// One fault of a failed result in "show results --failures", or the result itself when
// it recorded no fault.
static bool show_failure_row(FLFailureRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    char location[LOCATION_WIDTH + 1];
    if (row->source_file != NULL && row->source_line > 0) {
        char const *file = show_base_name(row->source_file);
        if (row->resource_address != 0) {
            // The stored address is 64-bit regardless of the width of the build reading
            // it, so it is formatted as a number rather than as a pointer.
            snprintf(location, sizeof location, "%s:%d @0x%llx", file, row->source_line,
                     (unsigned long long)row->resource_address);
        } else {
            snprintf(location, sizeof location, "%s:%d", file, row->source_line);
        }
    } else if (row->reason != NULL) {
        snprintf(location, sizeof location, "Reason: %.32s", row->reason);
    } else {
        snprintf(location, sizeof location, "%s", "No fault location recorded");
    }

    fprintf(printer->out, "%-4lld %-15.15s %-25.25s %-12s %-12s %-40.40s\n",
            (long long)row->run_id, row->suite_name, row->case_name,
            faultline_result_code_to_string(row->result_code),
            show_text_or(row->details, "no details"), location);
    printer->count++;
    return true;
}

static void show_failures(sqlite3 *db, char const *suite_name, int limit,
                          FLFailureScope scope) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    fprintf(printer.out, "\n=== Test Failures ===\n");
    if (suite_name != NULL) {
        fprintf(printer.out, "Suite: %s\n", suite_name);
    }
    if (scope == FL_FAILURES_LATEST) {
        fprintf(printer.out,
                "Showing failures from most recent test runs only. Use --all-history to "
                "see all failures.\n");
    }
    fprintf(printer.out, "%-4s %-15s %-25s %-12s %-12s %-40s\n", "Run", "Suite", "Test",
            "Result", "Details", "Allocation Location");
    fprintf(printer.out, "%-4s %-15s %-25s %-12s %-12s %-40s\n", "----", "---------------",
            "-------------------------", "------------", "------------",
            "----------------------------------------");

    faultline_failure_list(db, suite_name, limit, scope, show_failure_row, &printer);
}

// The heading of "show result <id>": the run's totals.
static bool show_run_detail(FLRunRow const *row, void *user) {
    FILE *out = ((ShowPrinter *)user)->out;

    fprintf(out, "\n=== Test Run Details (ID: %lld) ===\n", (long long)row->id);
    fprintf(out, "Suite: %s\n", row->suite_name);
    fprintf(out, "Timestamp: %s\n", row->started_at);
    fprintf(out, "Test Cases: %lld\n", (long long)row->case_count);
    fprintf(out, "Tests Run: %lld\n", (long long)row->cases_run);
    fprintf(out, "Tests Passed: %lld (%.1f%%)\n", (long long)row->passed,
            row->pass_rate * 100.0);
    fprintf(out, "Total Runtime: %.2f seconds\n", row->elapsed_seconds);
    fprintf(out, "Fault Sites Exercised: %lld\n", (long long)row->fault_sites);
    fprintf(out, "Discovery Failures: %lld\n", (long long)row->discovery_failures);
    fprintf(out, "Injection Failures: %lld\n", (long long)row->injection_failures);
    return true;
}

// One case's result in "show result <id>", with the location of its first fault.
static bool show_result_row(FLResultRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    char location[LOCATION_WIDTH + 1];
    if (row->source_file != NULL && row->source_line > 0) {
        snprintf(location, sizeof location, "%s:%d", show_base_name(row->source_file),
                 row->source_line);
    } else if (row->result_code > FL_PASS) {
        snprintf(location, sizeof location, "%s", "no location recorded");
    } else {
        location[0] = '\0';
    }

    fprintf(printer->out, "%-25.25s %-12s %8.3f %6lld %-50.50s %-40.40s %-40.40s\n",
            row->case_name, faultline_result_code_to_string(row->result_code),
            row->elapsed_seconds, (long long)row->fault_sites, show_text_or(row->reason, ""),
            show_text_or(row->details, "no details"), location);
    printer->count++;
    return true;
}

static void show_run_details(sqlite3 *db, int run_id) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    if (!faultline_run_find(db, run_id, show_run_detail, &printer)) {
        fprintf(printer.out, "Test run %d not found\n", run_id);
    } else {
        fprintf(printer.out, "\n--- Individual Test Results ---\n");
        fprintf(printer.out, "%-25s %-12s %-8s %-6s %-50s %-40s %-40s\n", "Test Name",
                "Result", "Time(s)", "Faults", "Reason", "Details", "Location");
        fprintf(printer.out, "%-25s %-12s %-8s %-6s %-50s %-40s %-40s\n",
                "-------------------------", "------------", "--------", "------",
                "--------------------------------------------------",
                "----------------------------------------",
                "----------------------------------------");

        faultline_result_list(db, run_id, show_result_row, &printer);
    }
}

// "show suite <name>": one suite's totals.
static bool show_suite_summary_row(FLSuiteRow const *row, void *user) {
    FILE *out = ((ShowPrinter *)user)->out;

    fprintf(out, "\n=== Suite Summary: %s ===\n", row->name);
    fprintf(out, "Total Runs: %lld\n", (long long)row->runs);
    if (row->runs > 0) {
        fprintf(out, "Average Pass Rate: %.1f%%\n", row->pass_rate * 100.0);
        fprintf(out, "First Run: %s\n", show_text_or(row->first_run_at, "-"));
        fprintf(out, "Last Run: %s\n", show_text_or(row->last_run_at, "-"));
        fprintf(out, "Total Fault Sites: %lld\n", (long long)row->fault_sites);
        fprintf(out, "Average Runtime: %.2f seconds\n", row->elapsed_seconds);
    } else {
        fprintf(out, "No runs found for this suite.\n");
    }
    return true;
}

static void show_suite_summary(sqlite3 *db, char const *suite_name) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    if (!faultline_suite_find(db, suite_name, show_suite_summary_row, &printer)) {
        fprintf(printer.out, "Suite '%s' not found\n", suite_name);
    }
}

// One suite in "show suites".
static bool show_suite_row(FLSuiteRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out, "%-30.30s %5lld %-19.19s %5.1f%% %7lld\n", row->name,
            (long long)row->runs, show_text_or(row->last_run_at, "-"),
            row->pass_rate * 100.0, (long long)row->fault_sites);
    printer->count++;
    return true;
}

// One suite in "show suites --verbose".
static bool show_suite_verbose_row(FLSuiteRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out, "%-30.30s %5lld %5lld %-19.19s %-19.19s %5.1f%% %7lld %8.3f %6lld\n",
            row->name, (long long)row->runs, (long long)row->cases,
            show_text_or(row->first_run_at, "-"), show_text_or(row->last_run_at, "-"),
            row->pass_rate * 100.0, (long long)row->fault_sites, row->elapsed_seconds,
            (long long)row->failures);
    printer->count++;
    return true;
}

static void show_suites(sqlite3 *db, int limit, bool verbose) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    fprintf(printer.out, "\n=== Test Suites ===\n");
    if (verbose) {
        fprintf(printer.out, "%-30s %5s %5s %-19s %-19s %6s %7s %8s %6s\n", "Suite", "Runs",
                "Cases", "First Run", "Last Run", "Pass%", "Faults", "Time(s)", "Fails");
        fprintf(printer.out, "%-30s %5s %5s %-19s %-19s %6s %7s %8s %6s\n",
                "------------------------------", "-----", "-----", "-------------------",
                "-------------------", "------", "-------", "--------", "------");
    } else {
        fprintf(printer.out, "%-30s %5s %-19s %6s %7s\n", "Suite", "Runs", "Last Run",
                "Pass%", "Faults");
        fprintf(printer.out, "%-30s %5s %-19s %6s %7s\n", "------------------------------",
                "-----", "-------------------", "------", "-------");
    }

    faultline_suite_list(db, limit, verbose ? show_suite_verbose_row : show_suite_row,
                         &printer);

    if (printer.count == 0) {
        fprintf(printer.out, "(no suites recorded)\n");
    }
}

// One case in "show cases".
static bool show_case_row(FLCaseRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out, "%-25.25s %-30.30s %5lld %5lld %7lld\n", row->suite_name,
            row->name, (long long)row->appearances, (long long)row->failures,
            (long long)row->last_fault_sites);
    printer->count++;
    return true;
}

// One case in "show cases --verbose", with its baseline beside its latest result.
static bool show_case_verbose_row(FLCaseRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out, "%-25.25s %-25.25s %5lld %5lld %5lld %5lld %8.3f %8.3f %-19.19s\n",
            row->suite_name, row->name, (long long)row->appearances,
            (long long)row->failures, (long long)row->baseline_fault_sites,
            (long long)row->last_fault_sites, row->baseline_elapsed_seconds,
            row->last_elapsed_seconds, show_text_or(row->baseline_set_at, "-"));
    printer->count++;
    return true;
}

static void show_cases(sqlite3 *db, char const *suite_name, int limit, bool verbose) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    fprintf(printer.out, "\n=== Test Cases ===\n");
    if (suite_name != NULL) {
        fprintf(printer.out, "Suite: %s\n", suite_name);
    }
    if (verbose) {
        fprintf(printer.out, "%-25s %-25s %5s %5s %5s %5s %8s %8s %-19s\n", "Suite", "Test",
                "Runs", "Fails", "Base", "Last", "BTime", "LTime", "Baseline");
        fprintf(printer.out, "%-25s %-25s %5s %5s %5s %5s %8s %8s %-19s\n",
                "-------------------------", "-------------------------", "-----",
                "-----", "-----", "-----", "--------", "--------", "-------------------");
    } else {
        fprintf(printer.out, "%-25s %-30s %5s %5s %7s\n", "Suite", "Test", "Runs", "Fails",
                "Faults");
        fprintf(printer.out, "%-25s %-30s %5s %5s %7s\n", "-------------------------",
                "------------------------------", "-----", "-----", "-------");
    }

    faultline_case_list(db, suite_name, limit,
                        verbose ? show_case_verbose_row : show_case_row, &printer);

    if (printer.count == 0) {
        fprintf(printer.out, "(no cases recorded)\n");
    }
}

// One regression in "show regressions", with each change as a percentage of the
// baseline.
static bool show_regression_row(FLRegressionRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    double const sites_delta
        = (row->baseline_fault_sites != 0)
              ? ((double)(row->last_fault_sites - row->baseline_fault_sites)
                 / (double)row->baseline_fault_sites)
                    * 100.0
              : 0.0;
    double const time_delta
        = (row->baseline_elapsed_seconds > 0.0)
              ? ((row->last_elapsed_seconds - row->baseline_elapsed_seconds)
                 / row->baseline_elapsed_seconds)
                    * 100.0
              : 0.0;
    char const *type = (row->coverage_regression && row->runtime_regression)
                           ? "COVERAGE+RUNTIME"
                       : row->coverage_regression ? "COVERAGE"
                                                  : "RUNTIME";

    fprintf(printer->out, "%-20.20s %-24.24s %7lld %7lld %7.1f%%   %9.3f %9.3f %7.1f%%  %s\n",
            row->suite_name, row->case_name, (long long)row->baseline_fault_sites,
            (long long)row->last_fault_sites, sites_delta, row->baseline_elapsed_seconds,
            row->last_elapsed_seconds, time_delta, type);
    printer->count++;
    return true;
}

static void show_regressions(sqlite3 *db, char const *suite_name, int limit,
                             double threshold_pct) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    fprintf(printer.out, "\n=== Regressions vs Baseline (runtime threshold %.0f%%) ===\n",
            threshold_pct);
    fprintf(printer.out, "%-20s %-24s %7s %7s %8s   %9s %9s %8s  %s\n", "Suite", "Test",
            "BaseFS", "LastFS", "FS d%", "BaseT(s)", "LastT(s)", "T d%", "Type");
    fprintf(printer.out, "%-20s %-24s %7s %7s %8s   %9s %9s %8s  %s\n",
            "--------------------", "------------------------", "-------", "-------",
            "--------", "---------", "---------", "--------", "----");

    faultline_regression_list(db, suite_name, limit, threshold_pct, show_regression_row,
                              &printer);

    if (printer.count == 0) {
        fprintf(printer.out, "(no regressions)\n");
    }
}

// One suite-day in "show trends", with each change from the suite's previous day, or "-"
// on its first day.
static bool show_trend_row(FLTrendRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    char pass_delta[16];
    char time_delta[16];
    char fail_delta[16];
    if (row->has_previous) {
        snprintf(pass_delta, sizeof pass_delta, "%+.1f",
                 (row->pass_rate - row->previous_pass_rate) * 100.0);
        snprintf(time_delta, sizeof time_delta, "%+.3f",
                 row->elapsed_seconds - row->previous_elapsed_seconds);
        snprintf(fail_delta, sizeof fail_delta, "%+lld",
                 (long long)(row->failures - row->previous_failures));
    } else {
        snprintf(pass_delta, sizeof pass_delta, "%s", "-");
        snprintf(time_delta, sizeof time_delta, "%s", "-");
        snprintf(fail_delta, sizeof fail_delta, "%s", "-");
    }

    fprintf(printer->out, "%-20.20s %-10s %5lld  %6.1f%% %8s  %8.3f %8s  %6lld %6s\n",
            row->suite_name, row->day, (long long)row->runs, row->pass_rate * 100.0,
            pass_delta, row->elapsed_seconds, time_delta, (long long)row->failures,
            fail_delta);
    printer->count++;
    return true;
}

static void show_trends(sqlite3 *db, char const *suite_name, int limit) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    fprintf(printer.out, "\n=== Suite Trends (per day) ===\n");
    fprintf(printer.out, "%-20s %-10s %5s  %7s %8s  %8s %8s  %6s %6s\n", "Suite", "Day",
            "Runs", "Pass%", "Pass d", "Time(s)", "Time d", "Fails", "Fail d");
    fprintf(printer.out, "%-20s %-10s %5s  %7s %8s  %8s %8s  %6s %6s\n",
            "--------------------", "----------", "-----", "-------", "--------",
            "--------", "--------", "------", "------");

    faultline_trend_list(db, suite_name, limit, show_trend_row, &printer);

    if (printer.count == 0) {
        fprintf(printer.out, "(no trend data)\n");
    }
}

// One source location in "show hotspots".
static bool show_hotspot_row(FLHotspotRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out, "%6lld %6lld  %s:%d\n", (long long)row->faults,
            (long long)row->cases, row->source_file, row->source_line);
    printer->count++;
    return true;
}

static void show_hotspots(sqlite3 *db, char const *suite_name, int limit) {
    ShowPrinter printer = {.out = stdout, .count = 0};

    fprintf(printer.out, "\n=== Fault Hotspots ===\n");
    fprintf(printer.out, "%6s %6s  %s\n", "Faults", "Tests", "Location");
    fprintf(printer.out, "%6s %6s  %s\n", "------", "------", "--------");

    faultline_hotspot_list(db, suite_name, limit, show_hotspot_row, &printer);

    if (printer.count == 0) {
        fprintf(printer.out, "(no faults recorded)\n");
    }
}

// ---------------------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------------------

/**
 * @brief Handler for "show results" command
 *
 * Lists test results with optional filtering
 * Options: --failures, --suite <name>, --limit <N>, --type, --phase, --with-source
 */
COMMAND_HANDLER(show_results_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    // show_cmd dispatches here only when a database is open.
    // Extract options with defaults
    bool        failures_only = has_option(cmd, "failures");
    int         limit         = get_int_option(cmd, "limit", 10); // default 10
    char const *suite         = get_string_option(cmd, "suite", NULL);

    // Failure-specific options (auto-imply --failures via validation)
    // bool        show_source = has_option(cmd, "with-source");
    // char const *type_filter  = get_string_option(cmd, "type", NULL);
    // char const *phase_filter = get_string_option(cmd, "phase", NULL);

    if (failures_only) {
        // Show only failed runs
        show_failures(ectx->db, suite, limit, FL_FAILURES_LATEST);
    } else {
        // Show recent runs (default view)
        show_runs(ectx->db, suite, limit);
    }

    // Note: type_filter, phase_filter, and show_source will be used once
    // show_failures is enhanced to support these filters
}

/**
 * @brief Handler for "show result <id>" command
 *
 * Shows detailed information about a specific test run
 * Positional argument: run_id
 * Options: --format, --verbose
 */
COMMAND_HANDLER(show_result_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    // Validation ensures we have exactly 1 positional argument
    int run_id = atoi(cmd->args[0]);

    show_run_details(ectx->db, run_id);
}

/**
 * @brief Handler for "show suites" command
 *
 * Lists all registered test suites
 * Options: --limit <N>, --format
 */
COMMAND_HANDLER(show_suites_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    int  limit   = get_int_option(cmd, "limit", 10); // default 10
    bool verbose = has_option(cmd, "verbose");

    show_suites(ectx->db, limit, verbose);
}

/**
 * @brief Handler for "show suite <name>" command
 *
 * Shows detailed information about a specific test suite
 * Positional argument: suite_name
 * Options: --verbose, --format
 */
COMMAND_HANDLER(show_suite_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    // Validation ensures we have exactly 1 positional argument
    char const *suite_name = cmd->args[0];
    bool        verbose    = has_option(cmd, "verbose");

    show_suite_summary(ectx->db, suite_name);

    // Verbose mode lists the suite's test cases beneath the summary. This is the
    // "show cases" report scoped to one suite, so reuse it rather than duplicate.
    if (verbose) {
        show_cases(ectx->db, suite_name, 0, true);
    }
}

/**
 * @brief Handler for "show cases" command
 *
 * Searches the test case catalog
 * Options: --suite <name>, --limit <N>, --format
 */
COMMAND_HANDLER(show_cases_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    char const *suite   = get_string_option(cmd, "suite", NULL);
    int         limit   = get_int_option(cmd, "limit", 0); // 0 = unlimited
    bool        verbose = has_option(cmd, "verbose");

    show_cases(ectx->db, suite, limit, verbose);
}

/**
 * @brief Handler for "show hotspots" command
 *
 * Shows code locations that frequently fail
 * Options: --suite <name>, --limit <N>, --format, --verbose
 */
COMMAND_HANDLER(show_hotspots_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    char const *suite = get_string_option(cmd, "suite", NULL);
    int         limit = get_int_option(cmd, "limit", 0); // 0 = unlimited

    show_hotspots(ectx->db, suite, limit);
}

/**
 * @brief Handler for "show trends" command
 *
 * Shows performance trends over time
 * Options: --suite <name>, --limit <N>, --format
 */
COMMAND_HANDLER(show_trends_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    char const *suite = get_string_option(cmd, "suite", NULL);
    int         limit = get_int_option(cmd, "limit", 0); // 0 = unlimited

    show_trends(ectx->db, suite, limit);
}

/**
 * @brief Handler for "show regressions" command
 *
 * Reports tests whose fault-site coverage dropped or whose runtime regressed
 * past the threshold, compared to each test's baseline.
 * Options: --suite <name>, --limit <N>, --threshold <pct>
 */
COMMAND_HANDLER(show_regressions_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    char const *suite     = get_string_option(cmd, "suite", NULL);
    int         limit     = get_int_option(cmd, "limit", 0);      // 0 = unlimited
    int         threshold = get_int_option(cmd, "threshold", 20); // percent

    show_regressions(ectx->db, suite, limit, (double)threshold);
}

/**
 * @brief Handler for top-level "show" command
 *
 * Delegates to the subcommand handler, propagating the full ExecutionContext
 * so subcommand handlers can access db, fctx, arena, etc.
 */
COMMAND_HANDLER(show_cmd) {
    if (cmd->subcommand != NULL) {
        // Build a new ExecutionContext that wraps the subcommand's RuntimeCommand
        // but inherits all driver fields (db, fctx, arena, mem_ctx) from the parent.
        ExecutionContext *parent = (ExecutionContext *)cmd;
        require_database(parent, "show");

        ExecutionContext sub_ctx = *parent;
        sub_ctx.cmd              = *cmd->subcommand;
        sub_ctx.cmd.command->handler((RuntimeCommand *)&sub_ctx);
    } else {
        fprintf(stderr,
                "Error: 'show' requires a subcommand (results, result, suites, etc.)\n");
        fprintf(stderr, "Try 'faultline help show' for usage information\n");
    }
}
