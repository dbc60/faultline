/**
 * @file faultline_cli_commands.c
 * @author Douglas Cuthbertson
 * @brief The root of the command tree: what faultline can be asked to do.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Every command is a file-scope WBCommand, so building the tree allocates nothing. This
 * file holds the root and the flags every command inherits, the helpers the domain files
 * share, and help and version. Each domain file adds its commands to the root through one
 * faultline_cli_add_<domain>():
 *
 *   faultline
 *     run <suite.dll>...            faultline_cli_commands_run.c
 *     result list | show <run-id>   faultline_cli_commands_results.c
 *     failure list
 *     trend list
 *     suite list | show <name>      faultline_cli_commands_suites.c
 *     case list                     faultline_cli_commands_cases.c
 *     regression list
 *     baseline reset
 *     hotspot list                  faultline_cli_commands_faults.c
 *     help [command...]             this file
 *     version
 *
 * The domain files are not standalone translation units. They use the helpers defined
 * above their #include lines, and faultline_cli_root() uses what they define.
 */
#include <stdio.h>  // FILE, fprintf, fputs, snprintf
#include <string.h> // strlen, strrchr

#include "faultline_cli.h" // FLCliSession, faultline_cli_open, FL_DEFAULT_DB_PATH

#include <faultline/fl_abi.h>          // FLAbiInfo, fl_fill_abi_info, FL_VERSION_*
#include <faultline/fl_macros.h>       // FL_ARRAY_COUNT, FL_STR, FL_UNUSED
#include <faultline/fl_result_codes.h> // faultline_result_code_to_string, FL_PASS

// Rows a list prints when --limit is not given.
#define FL_CLI_DEFAULT_LIMIT 10

// A --limit of 0 lists every row.
#define FL_CLI_ALL_ROWS 0

// Percent over its baseline's run time at which a case counts as a runtime regression.
#define FL_CLI_DEFAULT_THRESHOLD 20

// A flag description that names its default: "Most runs to list (default 10)".
#define FL_CLI_DESC_DEFAULT(text, value) text " (default " FL_STR(value) ")"

// ---------------------------------------------------------------------------
// Run functions, declared before the tables that point at them
// ---------------------------------------------------------------------------

static WB_RUN_FN(run_help);
static WB_RUN_FN(run_version);

// ---------------------------------------------------------------------------
// The root
// ---------------------------------------------------------------------------

// Inherited by every command.
static WBFlag root_flags[] = {
    {.name      = "db",
     .shorthand = 'd',
     .type      = WB_FLAG_STRING,
     .desc      = "Results database file (default " FL_DEFAULT_DB_PATH ")",
     .def       = {.s = FL_DEFAULT_DB_PATH}},
    {.name      = "no-db",
     .shorthand = 0,
     .type      = WB_FLAG_BOOL,
     .desc      = "Use no database: run records nothing, and reports refuse to run",
     .def       = {.b = false}},
    {.name      = "log-level",
     .shorthand = 'l',
     .type      = WB_FLAG_STRING,
     .desc      = "Log verbosity: error, warn, info, or debug (default info)",
     .def       = {.s = NULL}},
    {.name      = "log-file",
     .shorthand = 0,
     .type      = WB_FLAG_STRING,
     .desc      = "File the log is written to",
     .def       = {.s = NULL}},
};

static WBCommand root_cmd = {
    .usage      = "faultline",
    .short_desc = "Fault injection testing",
    .long_desc  = "FaultLine runs test suites, failing each allocation a case makes\n"
                  "in turn, and records every run in a SQLite database. The other\n"
                  "commands report from that database.",
    .persistent_flags      = root_flags,
    .persistent_flag_count = FL_ARRAY_COUNT(root_flags),
};

// ---------------------------------------------------------------------------
// Report helpers
//
// Each report prints its heading, then hands a row printer to a DAL read. A row's
// strings are valid only while its printer runs, so the printers print them and keep
// nothing.
// ---------------------------------------------------------------------------

// Where the row printers write, and how many rows they have written.
typedef struct ShowPrinter {
    FILE *out;
    int   count;
} ShowPrinter;

// How much of each row a report prints.
typedef enum ShowDetail {
    SHOW_BRIEF,  // the default columns
    SHOW_VERBOSE // every column
} ShowDetail;

// --verbose as a report's detail.
static ShowDetail show_detail(WBCommand const *cmd) {
    return wb_flag_bool(cmd, "verbose") ? SHOW_VERBOSE : SHOW_BRIEF;
}

// A column that may be NULL, with a fallback to print in its place.
static char const *show_text_or(char const *text, char const *fallback) {
    return (text != NULL) ? text : fallback;
}

// ---------------------------------------------------------------------------
// The domain files, each adding its commands through faultline_cli_add_<domain>()
// ---------------------------------------------------------------------------

#include "faultline_cli_commands_run.c"
#include "faultline_cli_commands_results.c"
#include "faultline_cli_commands_cases.c"
#include "faultline_cli_commands_suites.c" // after cases: "suite show -v" uses show_cases
#include "faultline_cli_commands_faults.c"

// ---------------------------------------------------------------------------
// help and version
// ---------------------------------------------------------------------------

static WBCommand help_cmd = {
    .usage      = "help [command...]",
    .short_desc = "Show help for faultline or one of its commands",
    .example    = "  help\n"
                  "  help result show",
    .max_args   = WB_ARGS_ANY,
    .run        = run_help,
};

static WBCommand version_cmd = {
    .usage      = "version",
    .short_desc = "Print the version and the build identity suites are checked against",
    .run        = run_version,
};

WBCommand *faultline_cli_root(void) {
    static bool wired = false;

    // wb_command_add appends, so wiring a second time would list every command twice.
    if (!wired) {
        faultline_cli_add_run(&root_cmd);
        faultline_cli_add_results(&root_cmd);
        faultline_cli_add_suites(&root_cmd);
        faultline_cli_add_cases(&root_cmd);
        faultline_cli_add_faults(&root_cmd);

        wb_command_add(&root_cmd, &help_cmd);
        wb_command_add(&root_cmd, &version_cmd);
        wired = true;
    }

    return &root_cmd;
}

static WB_RUN_FN(run_help) {
    FL_UNUSED(cmd);
    FL_UNUSED(user);

    int              consumed = 0;
    WBCommand const *target   = wb_command_find(&root_cmd, argc, argv, &consumed);

    int status = WB_CLI_OK;
    if (consumed < argc) {
        fprintf(io->err, "unknown command \"%s\"\n", argv[consumed]);
        status = WB_CLI_USAGE;
    } else {
        wb_command_help(target, io);
    }

    return status;
}

static WB_RUN_FN(run_version) {
    FL_UNUSED(cmd);
    FL_UNUSED(argc);
    FL_UNUSED(argv);
    FL_UNUSED(user);

    fprintf(io->out, "FaultLine version %d.%d.%d\n", FL_VERSION_MAJOR, FL_VERSION_MINOR,
            FL_VERSION_PATCH);
    fputs("Fault Injection Testing Framework\n", io->out);
    fputs("Copyright (c) 2025 Douglas Cuthbertson\n", io->out);

    // A suite and this driver must agree on what crosses between them. Printing the
    // identity used for that comparison is what makes a reported mismatch diagnosable:
    // run this, compare it with what the suite reported, rebuild the odd one out.
    FLAbiInfo abi;
    fl_fill_abi_info(&abi);
    fputs("\nBuild identity\n", io->out);
    fprintf(io->out, "  compiler       %s %u\n", fl_abi_compiler_str(abi.compiler_id),
            abi.compiler_version);
    fprintf(io->out, "  c runtime      %s\n", fl_abi_crt_str(abi.crt_id));
    fprintf(io->out, "  compatibility  v%u\n", abi.compatibility_version);

    return WB_CLI_OK;
}
