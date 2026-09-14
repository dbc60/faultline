/**
 * @file faultline_cli_commands_cases.c
 * @author Douglas Cuthbertson
 * @brief The "case", "regression", and "baseline" commands, over the cases slice of the
 * DAL.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit: faultline_cli_commands.c #includes it after the
 * helpers it uses.
 */
#include <faultline_dal_cases.h> // faultline_case_list, faultline_regression_list, ...

// ---------------------------------------------------------------------------
// Run functions, declared before the tables that point at them
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_case_list);
static WB_RUN_FN(run_regression_list);
static WB_RUN_FN(run_baseline_reset);

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------

static WBFlag case_list_flags[] = {
    {.name      = "suite",
     .shorthand = 's',
     .type      = WB_FLAG_STRING,
     .desc      = "Only this suite's cases",
     .def       = {.s = NULL}},
    {.name      = "limit",
     .shorthand = 'n',
     .type      = WB_FLAG_I64,
     .desc      = "Most cases to list; 0, the default, lists all",
     .def       = {.i = FL_CLI_ALL_ROWS}},
    {.name      = "verbose",
     .shorthand = 'v',
     .type      = WB_FLAG_BOOL,
     .desc      = "Add each case's baseline beside its latest run",
     .def       = {.b = false}},
};

static WBFlag regression_list_flags[] = {
    {.name      = "suite",
     .shorthand = 's',
     .type      = WB_FLAG_STRING,
     .desc      = "Only this suite's cases",
     .def       = {.s = NULL}},
    {.name      = "limit",
     .shorthand = 'n',
     .type      = WB_FLAG_I64,
     .desc      = "Most cases to list; 0, the default, lists all",
     .def       = {.i = FL_CLI_ALL_ROWS}},
    {.name      = "threshold",
     .shorthand = 't',
     .type      = WB_FLAG_I64,
     .desc      = FL_CLI_DESC_DEFAULT("Percent slower than the baseline that counts",
                                      FL_CLI_DEFAULT_THRESHOLD),
     .def       = {.i = FL_CLI_DEFAULT_THRESHOLD}},
};

static WBFlag baseline_reset_flags[] = {
    {.name      = "suite",
     .shorthand = 's',
     .type      = WB_FLAG_STRING,
     .desc      = "Only this suite's cases",
     .def       = {.s = NULL}},
    {.name      = "case",
     .shorthand = 'c',
     .type      = WB_FLAG_STRING,
     .desc      = "Only cases with this name",
     .def       = {.s = NULL}},
};

// ---------------------------------------------------------------------------
// The commands
// ---------------------------------------------------------------------------

static WBCommand case_cmd = {
    .usage      = "case",
    .short_desc = "List each case's history",
};

static WBCommand case_list_cmd = {
    .usage      = "list",
    .short_desc = "List cases with how often each ran and failed",
    .long_desc  = "Ordered by suite, then name. --verbose adds the fault sites and run\n"
                  "time of each case's baseline beside those of its latest run.",
    .flags      = case_list_flags,
    .flag_count = FL_ARRAY_COUNT(case_list_flags),
    .run        = run_case_list,
};

static WBCommand regression_cmd = {
    .usage      = "regression",
    .short_desc = "List cases that regressed against their baseline",
};

static WBCommand regression_list_cmd = {
    .usage      = "list",
    .short_desc = "List cases whose latest run lost fault sites or ran slower",
    .long_desc  = "A case whose latest run exercised fewer fault sites than its\n"
                  "baseline is a coverage regression. One that ran more than\n"
                  "--threshold percent slower is a runtime regression. The largest\n"
                  "loss of coverage comes first.",
    .example    = "  regression list -t 50",
    .flags      = regression_list_flags,
    .flag_count = FL_ARRAY_COUNT(regression_list_flags),
    .run        = run_regression_list,
};

static WBCommand baseline_cmd = {
    .usage      = "baseline",
    .short_desc = "Set the runs that regressions are compared against",
};

static WBCommand baseline_reset_cmd = {
    .usage      = "reset",
    .short_desc = "Make each matching case's latest run its baseline",
    .long_desc  = "With neither flag, every case's latest run becomes its baseline.",
    .example    = "  baseline reset\n"
                  "  baseline reset -s \"Faultline Driver\"",
    .flags      = baseline_reset_flags,
    .flag_count = FL_ARRAY_COUNT(baseline_reset_flags),
    .run        = run_baseline_reset,
};

static void faultline_cli_add_cases(WBCommand *root) {
    wb_command_add(root, &case_cmd);
    wb_command_add(&case_cmd, &case_list_cmd);

    wb_command_add(root, &regression_cmd);
    wb_command_add(&regression_cmd, &regression_list_cmd);

    wb_command_add(root, &baseline_cmd);
    wb_command_add(&baseline_cmd, &baseline_reset_cmd);
}

// ---------------------------------------------------------------------------
// Reports
// ---------------------------------------------------------------------------

// One case in "case list".
static bool show_case_row(FLCaseRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out, "%-25.25s %-30.30s %5lld %5lld %7lld\n", row->suite_name,
            row->name, (long long)row->appearances, (long long)row->failures,
            (long long)row->last_fault_sites);
    printer->count++;
    return true;
}

// One case in "case list --verbose", with its baseline beside its latest result.
static bool show_case_verbose_row(FLCaseRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out,
            "%-25.25s %-25.25s %5lld %5lld %5lld %5lld %8.3f %8.3f %-19.19s\n",
            row->suite_name, row->name, (long long)row->appearances,
            (long long)row->failures, (long long)row->baseline_fault_sites,
            (long long)row->last_fault_sites, row->baseline_elapsed_seconds,
            row->last_elapsed_seconds, show_text_or(row->baseline_set_at, "-"));
    printer->count++;
    return true;
}

static void show_cases(FILE *out, sqlite3 *db, char const *suite_name, i64 limit,
                       ShowDetail detail) {
    ShowPrinter printer = {.out = out, .count = 0};

    fprintf(printer.out, "\n=== Test Cases ===\n");
    if (suite_name != NULL) {
        fprintf(printer.out, "Suite: %s\n", suite_name);
    }
    if (detail == SHOW_VERBOSE) {
        fprintf(printer.out, "%-25s %-25s %5s %5s %5s %5s %8s %8s %-19s\n", "Suite",
                "Test", "Runs", "Fails", "Base", "Last", "BTime", "LTime", "Baseline");
        fprintf(printer.out, "%-25s %-25s %5s %5s %5s %5s %8s %8s %-19s\n",
                "-------------------------", "-------------------------", "-----",
                "-----", "-----", "-----", "--------", "--------", "-------------------");
    } else {
        fprintf(printer.out, "%-25s %-30s %5s %5s %7s\n", "Suite", "Test", "Runs",
                "Fails", "Faults");
        fprintf(printer.out, "%-25s %-30s %5s %5s %7s\n", "-------------------------",
                "------------------------------", "-----", "-----", "-------");
    }

    faultline_case_list(db, suite_name, limit,
                        (detail == SHOW_VERBOSE) ? show_case_verbose_row : show_case_row,
                        &printer);

    if (printer.count == 0) {
        fprintf(printer.out, "(no cases recorded)\n");
    }
}

// One regression in "regression list", with each change as a percentage of the
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

    fprintf(printer->out,
            "%-20.20s %-24.24s %7lld %7lld %7.1f%%   %9.3f %9.3f %7.1f%%  %s\n",
            row->suite_name, row->case_name, (long long)row->baseline_fault_sites,
            (long long)row->last_fault_sites, sites_delta, row->baseline_elapsed_seconds,
            row->last_elapsed_seconds, time_delta, type);
    printer->count++;
    return true;
}

static void show_regressions(FILE *out, sqlite3 *db, char const *suite_name, i64 limit,
                             double threshold_pct) {
    ShowPrinter printer = {.out = out, .count = 0};

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

// ---------------------------------------------------------------------------
// Run functions
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_case_list) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);
    FL_UNUSED(argv);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    show_cases(io->out, db, wb_flag_string(cmd, "suite"), wb_flag_i64(cmd, "limit"),
               show_detail(cmd));
    return WB_CLI_OK;
}

static WB_RUN_FN(run_regression_list) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);
    FL_UNUSED(argv);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    char const  *suite         = wb_flag_string(cmd, "suite");
    double const threshold_pct = (double)wb_flag_i64(cmd, "threshold");
    show_regressions(io->out, db, suite, wb_flag_i64(cmd, "limit"), threshold_pct);
    return WB_CLI_OK;
}

static WB_RUN_FN(run_baseline_reset) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);
    FL_UNUSED(argv);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    i64 const n = faultline_baseline_reset(db, wb_flag_string(cmd, "suite"),
                                           wb_flag_string(cmd, "case"));
    fprintf(io->out, "Reset %lld baseline%s to the latest run.\n", (long long)n,
            (n == 1) ? "" : "s");
    return WB_CLI_OK;
}
