/**
 * @file faultline_cli_commands_faults.c
 * @author Douglas Cuthbertson
 * @brief The "hotspot" command, over the faults slice of the DAL.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit: faultline_cli_commands.c #includes it after the
 * helpers it uses.
 */
#include <faultline_dal_faults.h> // faultline_hotspot_list

// ---------------------------------------------------------------------------
// Run functions, declared before the tables that point at them
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_hotspot_list);

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------

static WBFlag hotspot_list_flags[] = {
    {.name      = "suite",
     .shorthand = 's',
     .type      = WB_FLAG_STRING,
     .desc      = "Only this suite's faults",
     .def       = {.s = NULL}},
    {.name      = "limit",
     .shorthand = 'n',
     .type      = WB_FLAG_I64,
     .desc      = "Most locations to list; 0, the default, lists all",
     .def       = {.i = FL_CLI_ALL_ROWS}},
};

// ---------------------------------------------------------------------------
// The commands
// ---------------------------------------------------------------------------

static WBCommand hotspot_cmd = {
    .usage      = "hotspot",
    .short_desc = "List the source locations with the most faults",
};

static WBCommand hotspot_list_cmd = {
    .usage      = "list",
    .short_desc = "List source locations, the most faults first",
    .long_desc  = "A fault recorded without a source file is left out.",
    .flags      = hotspot_list_flags,
    .flag_count = FL_ARRAY_COUNT(hotspot_list_flags),
    .run        = run_hotspot_list,
};

static void faultline_cli_add_faults(WBCommand *root) {
    wb_command_add(root, &hotspot_cmd);
    wb_command_add(&hotspot_cmd, &hotspot_list_cmd);
}

// ---------------------------------------------------------------------------
// Reports
// ---------------------------------------------------------------------------

// One source location in "hotspot list".
static bool show_hotspot_row(FLHotspotRow const *row, void *user) {
    ShowPrinter *printer = (ShowPrinter *)user;

    fprintf(printer->out, "%6lld %6lld  %s:%d\n", (long long)row->faults,
            (long long)row->cases, row->source_file, row->source_line);
    printer->count++;
    return true;
}

static void show_hotspots(FILE *out, sqlite3 *db, char const *suite_name, i64 limit) {
    ShowPrinter printer = {.out = out, .count = 0};

    fprintf(printer.out, "\n=== Fault Hotspots ===\n");
    fprintf(printer.out, "%6s %6s  %s\n", "Faults", "Tests", "Location");
    fprintf(printer.out, "%6s %6s  %s\n", "------", "------", "--------");

    faultline_hotspot_list(db, suite_name, limit, show_hotspot_row, &printer);

    if (printer.count == 0) {
        fprintf(printer.out, "(no faults recorded)\n");
    }
}

// ---------------------------------------------------------------------------
// Run functions
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_hotspot_list) {
    FLCliSession *session = (FLCliSession *)user;
    FL_UNUSED(argc);
    FL_UNUSED(argv);

    sqlite3 *db = faultline_cli_open(cmd, session, FL_CLI_DB_REQUIRED);
    if (db == NULL) {
        return WB_CLI_ERROR;
    }

    show_hotspots(io->out, db, wb_flag_string(cmd, "suite"), wb_flag_i64(cmd, "limit"));
    return WB_CLI_OK;
}
