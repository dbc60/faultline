/**
 * @file faultline_cli_commands_suites.c
 * @author Douglas Cuthbertson
 * @brief The "suite" commands, over the suites slice of the DAL.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit: faultline_cli_commands.c #includes it after the
 * helpers it uses and after the case commands, whose report "suite show -v" prints.
 */
#include <faultline_dal_suites.h> // faultline_suite_list, faultline_suite_find

// ---------------------------------------------------------------------------
// Run functions, declared before the tables that point at them
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_suite_list);
static WB_RUN_FN(run_suite_show);

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------

static WBFlag suite_list_flags[] = {
    {.name      = "limit",
     .shorthand = 'n',
     .type      = WB_FLAG_I64,
     .desc
     = FL_CLI_DESC_DEFAULT("Most suites to list; 0 lists all", FL_CLI_DEFAULT_LIMIT),
     .def = {.i = FL_CLI_DEFAULT_LIMIT}},
    {.name      = "verbose",
     .shorthand = 'v',
     .type      = WB_FLAG_BOOL,
     .desc      = "Add each suite's case count, first run, run time, and failures",
     .def       = {.b = false}},
};

static WBFlag suite_show_flags[] = {
    {.name      = "verbose",
     .shorthand = 'v',
     .type      = WB_FLAG_BOOL,
     .desc      = "List the suite's cases beneath its totals",
     .def       = {.b = false}},
};

// ---------------------------------------------------------------------------
// The commands
// ---------------------------------------------------------------------------

static WBCommand suite_cmd = {
    .usage      = "suite",
    .short_desc = "List suites and show one suite's totals",
};

static WBCommand suite_list_cmd = {
    .usage      = "list",
    .short_desc = "List suites, the most recently run first",
    .flags      = suite_list_flags,
    .flag_count = FL_ARRAY_COUNT(suite_list_flags),
    .run        = run_suite_list,
};

static WBCommand suite_show_cmd = {
    .usage      = "show <name>",
    .short_desc = "Show one suite's totals over all of its runs",
    .example    = "  suite show \"Faultline Driver\" -v",
    .flags      = suite_show_flags,
    .flag_count = FL_ARRAY_COUNT(suite_show_flags),
    .min_args   = 1,
    .max_args   = 1,
    .run        = run_suite_show,
};

static void faultline_cli_add_suites(WBCommand *root) {
    wb_command_add(root, &suite_cmd);
    wb_command_add(&suite_cmd, &suite_list_cmd);
    wb_command_add(&suite_cmd, &suite_show_cmd);
}

// ---------------------------------------------------------------------------
// Reports
// ---------------------------------------------------------------------------

// "suite show <name>": one suite's totals.
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

// False when no suite has the name.
static bool show_suite_summary(FILE *out, sqlite3 *db, char const *suite_name) {
    ShowPrinter printer = {.out = out, .count = 0};
    return faultline_suite_find(db, suite_name, show_suite_summary_row, &printer);
}

// One suite in "suite list".
static bool show_suite_row(FLSuiteRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out, "%-30.30s %5lld %-19.19s %5.1f%% %7lld\n", row->name,
            (long long)row->runs, show_text_or(row->last_run_at, "-"),
            row->pass_rate * 100.0, (long long)row->fault_sites);
    printer->count++;
    return true;
}

// One suite in "suite list --verbose".
static bool show_suite_verbose_row(FLSuiteRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out,
            "%-30.30s %5lld %5lld %-19.19s %-19.19s %5.1f%% %7lld %8.3f %6lld\n",
            row->name, (long long)row->runs, (long long)row->cases,
            show_text_or(row->first_run_at, "-"), show_text_or(row->last_run_at, "-"),
            row->pass_rate * 100.0, (long long)row->fault_sites, row->elapsed_seconds,
            (long long)row->failures);
    printer->count++;
    return true;
}

static void show_suites(FILE *out, sqlite3 *db, i64 limit, ShowDetail detail) {
    ShowPrinter printer = {.out = out, .count = 0};

    fprintf(printer.out, "\n=== Test Suites ===\n");
    if (detail == SHOW_VERBOSE) {
        fprintf(printer.out, "%-30s %5s %5s %-19s %-19s %6s %7s %8s %6s\n", "Suite",
                "Runs", "Cases", "First Run", "Last Run", "Pass%", "Faults", "Time(s)",
                "Fails");
        fprintf(printer.out, "%-30s %5s %5s %-19s %-19s %6s %7s %8s %6s\n",
                "------------------------------", "-----", "-----", "-------------------",
                "-------------------", "------", "-------", "--------", "------");
    } else {
        fprintf(printer.out, "%-30s %5s %-19s %6s %7s\n", "Suite", "Runs", "Last Run",
                "Pass%", "Faults");
        fprintf(printer.out, "%-30s %5s %-19s %6s %7s\n",
                "------------------------------", "-----", "-------------------",
                "------", "-------");
    }

    faultline_suite_list(db, limit,
                         (detail == SHOW_VERBOSE) ? show_suite_verbose_row
                                                  : show_suite_row,
                         &printer);

    if (printer.count == 0) {
        fprintf(printer.out, "(no suites recorded)\n");
    }
}

// ---------------------------------------------------------------------------
// Run functions
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_suite_list) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);
    FL_UNUSED(argv);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    show_suites(io->out, db, wb_flag_i64(cmd, "limit"), show_detail(cmd));
    return WB_CLI_OK;
}

static WB_RUN_FN(run_suite_show) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    int status = WB_CLI_OK;
    if (!show_suite_summary(io->out, db, argv[0])) {
        fprintf(io->err, "no suite is named \"%s\"\n", argv[0]);
        status = WB_CLI_ERROR;
    } else if (show_detail(cmd) == SHOW_VERBOSE) {
        // The "case list --verbose" report, scoped to this suite.
        show_cases(io->out, db, argv[0], FL_CLI_ALL_ROWS, SHOW_VERBOSE);
    }

    return status;
}
