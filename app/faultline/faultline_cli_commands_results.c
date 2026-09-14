/**
 * @file faultline_cli_commands_results.c
 * @author Douglas Cuthbertson
 * @brief The "result", "failure", and "trend" commands, over the runs slice of the DAL.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit: faultline_cli_commands.c #includes it after the
 * helpers it uses.
 */
#include <faultline_dal_runs.h> // faultline_run_list, faultline_run_find, ...

enum {
    RUN_SUITE_WIDTH = 20, // characters in the Suite column of "result list"
    ELLIPSIS_LENGTH = 3,  // characters in the "..." that ends a shortened suite name
    LOCATION_WIDTH  = 40  // characters in the location column of a failure or result
};

// ---------------------------------------------------------------------------
// Run functions, declared before the tables that point at them
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_result_list);
static WB_RUN_FN(run_result_show);
static WB_RUN_FN(run_failure_list);
static WB_RUN_FN(run_trend_list);

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------

static WBFlag result_list_flags[] = {
    {.name      = "suite",
     .shorthand = 's',
     .type      = WB_FLAG_STRING,
     .desc      = "Only this suite's runs",
     .def       = {.s = NULL}},
    {.name      = "limit",
     .shorthand = 'n',
     .type      = WB_FLAG_I64,
     .desc = FL_CLI_DESC_DEFAULT("Most runs to list; 0 lists all", FL_CLI_DEFAULT_LIMIT),
     .def  = {.i = FL_CLI_DEFAULT_LIMIT}},
};

static WBFlag failure_list_flags[] = {
    {.name      = "suite",
     .shorthand = 's',
     .type      = WB_FLAG_STRING,
     .desc      = "Only this suite's failures",
     .def       = {.s = NULL}},
    {.name      = "limit",
     .shorthand = 'n',
     .type      = WB_FLAG_I64,
     .desc = FL_CLI_DESC_DEFAULT("Most rows to list; 0 lists all", FL_CLI_DEFAULT_LIMIT),
     .def  = {.i = FL_CLI_DEFAULT_LIMIT}},
};

static WBFlag trend_list_flags[] = {
    {.name      = "suite",
     .shorthand = 's',
     .type      = WB_FLAG_STRING,
     .desc      = "Only this suite's days",
     .def       = {.s = NULL}},
    {.name      = "limit",
     .shorthand = 'n',
     .type      = WB_FLAG_I64,
     .desc      = "Most rows to list; 0, the default, lists all",
     .def       = {.i = FL_CLI_ALL_ROWS}},
};

// ---------------------------------------------------------------------------
// The commands
// ---------------------------------------------------------------------------

static WBCommand result_cmd = {
    .usage      = "result",
    .short_desc = "List recorded runs and show one in full",
    .long_desc  = "A run is one suite's pass through its cases, recorded with each\n"
                  "case's result.",
};

static WBCommand result_list_cmd = {
    .usage      = "list",
    .short_desc = "List runs, newest first",
    .example    = "  result list\n"
                  "  result list -s \"Faultline Driver\" -n 0",
    .flags      = result_list_flags,
    .flag_count = FL_ARRAY_COUNT(result_list_flags),
    .run        = run_result_list,
};

static WBCommand result_show_cmd = {
    .usage      = "show <run-id>",
    .short_desc = "Show one run's totals and each case's result",
    .example    = "  result show 12",
    .min_args   = 1,
    .max_args   = 1,
    .run        = run_result_show,
};

static WBCommand failure_cmd = {
    .usage      = "failure",
    .short_desc = "List the failures of each suite's latest run",
};

static WBCommand failure_list_cmd = {
    .usage      = "list",
    .short_desc = "List the faults of each failed result in each suite's latest run",
    .long_desc = "One row per fault. A failed result that recorded no fault is one row,\n"
                 "with its reason in place of a location.",
    .flags     = failure_list_flags,
    .flag_count = FL_ARRAY_COUNT(failure_list_flags),
    .run        = run_failure_list,
};

static WBCommand trend_cmd = {
    .usage      = "trend",
    .short_desc = "List each suite's totals per day",
};

static WBCommand trend_list_cmd = {
    .usage      = "list",
    .short_desc = "List each suite's totals per day, with the change from the day before",
    .long_desc  = "Suites in name order, each suite's days newest first. A suite's\n"
                  "first day has nothing to change from, so its changes print as \"-\".",
    .flags      = trend_list_flags,
    .flag_count = FL_ARRAY_COUNT(trend_list_flags),
    .run        = run_trend_list,
};

static void faultline_cli_add_results(WBCommand *root) {
    wb_command_add(root, &result_cmd);
    wb_command_add(&result_cmd, &result_list_cmd);
    wb_command_add(&result_cmd, &result_show_cmd);

    wb_command_add(root, &failure_cmd);
    wb_command_add(&failure_cmd, &failure_list_cmd);

    wb_command_add(root, &trend_cmd);
    wb_command_add(&trend_cmd, &trend_list_cmd);
}

// ---------------------------------------------------------------------------
// Reports
// ---------------------------------------------------------------------------

// The file name at the end of a path.
static char const *show_base_name(char const *path) {
    char const *name = strrchr(path, '\\');
    if (name == NULL) {
        name = strrchr(path, '/');
    }
    return (name != NULL) ? name + 1 : path;
}

// One run in "result list".
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

static void show_runs(FILE *out, sqlite3 *db, char const *suite_name, i64 limit) {
    ShowPrinter printer = {.out = out, .count = 0};

    fprintf(printer.out, "\n=== Recent Test Runs ===\n");
    if (suite_name != NULL) {
        fprintf(printer.out, "Suite: %s\n", suite_name);
    }
    fprintf(printer.out, "%-4s %-20s %-19s %-6s %-7s %-4s/%-4s %-8s %-6s\n", "ID",
            "Suite", "Timestamp", "Cases", "Faults", "Pass", "Run", "Time(s)", "Rate%");
    fprintf(printer.out, "%-4s %-20s %-19s %-6s %-7s %-9s %-8s %-6s\n", "----",
            "--------------------", "-------------------", "------", "-------",
            "---------", "--------", "------");

    faultline_run_list(db, suite_name, limit, show_run_row, &printer);
}

// One fault of a failed result in "failure list", or the result itself when it recorded
// no fault.
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

static void show_failures(FILE *out, sqlite3 *db, char const *suite_name, i64 limit) {
    ShowPrinter printer = {.out = out, .count = 0};

    fprintf(printer.out, "\n=== Test Failures ===\n");
    if (suite_name != NULL) {
        fprintf(printer.out, "Suite: %s\n", suite_name);
    }
    fprintf(printer.out, "From each suite's latest run.\n");
    fprintf(printer.out, "%-4s %-15s %-25s %-12s %-12s %-40s\n", "Run", "Suite", "Test",
            "Result", "Details", "Allocation Location");
    fprintf(printer.out, "%-4s %-15s %-25s %-12s %-12s %-40s\n", "----",
            "---------------", "-------------------------", "------------",
            "------------", "----------------------------------------");

    faultline_failure_list(db, suite_name, limit, FL_FAILURES_LATEST, show_failure_row,
                           &printer);
}

// The heading of "result show <run-id>": the run's totals.
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

// One case's result in "result show <run-id>", with the location of its first fault.
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
            row->elapsed_seconds, (long long)row->fault_sites,
            show_text_or(row->reason, ""), show_text_or(row->details, "no details"),
            location);
    printer->count++;
    return true;
}

// The run's totals, then each case's result. False when no run has the id.
static bool show_run_details(FILE *out, sqlite3 *db, i64 run_id) {
    ShowPrinter printer = {.out = out, .count = 0};

    if (!faultline_run_find(db, run_id, show_run_detail, &printer)) {
        return false;
    }

    fprintf(printer.out, "\n--- Individual Test Results ---\n");
    fprintf(printer.out, "%-25s %-12s %-8s %-6s %-50s %-40s %-40s\n", "Test Name",
            "Result", "Time(s)", "Faults", "Reason", "Details", "Location");
    fprintf(printer.out, "%-25s %-12s %-8s %-6s %-50s %-40s %-40s\n",
            "-------------------------", "------------", "--------", "------",
            "--------------------------------------------------",
            "----------------------------------------",
            "----------------------------------------");

    faultline_result_list(db, run_id, show_result_row, &printer);
    return true;
}

// One suite-day in "trend list", with each change from the suite's previous day, or "-"
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

static void show_trends(FILE *out, sqlite3 *db, char const *suite_name, i64 limit) {
    ShowPrinter printer = {.out = out, .count = 0};

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

// ---------------------------------------------------------------------------
// Run functions
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_result_list) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);
    FL_UNUSED(argv);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    show_runs(io->out, db, wb_flag_string(cmd, "suite"), wb_flag_i64(cmd, "limit"));
    return WB_CLI_OK;
}

static WB_RUN_FN(run_result_show) {
    FLCliSession *session = (FLCliSession *)user;
    i64           run_id  = 0;
    FL_UNUSED(argc);

    if (!wb_parse_i64(argv[0], &run_id) || run_id <= 0) {
        fprintf(io->err, "\"%s\" is not a run id\n", argv[0]);
        return WB_CLI_USAGE;
    }

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    int status = WB_CLI_OK;
    if (!show_run_details(io->out, db, run_id)) {
        fprintf(io->err, "no run has id %lld\n", (long long)run_id);
        status = WB_CLI_ERROR;
    }

    return status;
}

static WB_RUN_FN(run_failure_list) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);
    FL_UNUSED(argv);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    show_failures(io->out, db, wb_flag_string(cmd, "suite"), wb_flag_i64(cmd, "limit"));
    return WB_CLI_OK;
}

static WB_RUN_FN(run_trend_list) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);
    FL_UNUSED(argv);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    show_trends(io->out, db, wb_flag_string(cmd, "suite"), wb_flag_i64(cmd, "limit"));
    return WB_CLI_OK;
}
