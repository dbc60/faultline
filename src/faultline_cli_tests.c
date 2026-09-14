/**
 * @file faultline_cli_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the command tree: what faultline prints when it is asked.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Each case records runs into a database of its own, runs a command line through
 * faultline_cli_run_once() as the executable does, and reads back the status and what
 * the command printed.
 */
#include <stdio.h>  // FILE, fclose, fflush, fopen_s, fread, fseek, remove, snprintf
#include <stdlib.h> // NULL
#include <string.h> // memcpy, strlen, strstr
#include <time.h>   // time_t

// The command tree and the library under it.
#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "fault_injector.c"
#include "fla_memory_service.c"
#include "faultline_context.c"
#include "faultline_driver.c"
#include "faultline_sqlite.c"
#include "faultline_dal.c"
#include "fl_exception.c"
#include "fla_log_service.c"
#include "fla_timer_service.c"
#include "fla_file_service.c"
#include "fla_stream_service.c"
#include "fnv/FNV64.c"
#include "output_junit.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"
#include "lock_os.c"
#include "set.c"
#include "win_timer.c"
#include "wb_cli.c"
#include "faultline_cli_commands.c"
#include "faultline_cli.c"

#include <faultline/fault.h>                // FAULT_NO_RESOURCE
#include <faultline/fl_abbreviated_types.h> // i64, u32
#include <faultline/fl_exception_assert.h>  // FL_ASSERT_*
#include <faultline/fl_macros.h>            // FL_ARRAY_COUNT, FL_CONTAINER_OF, FL_STR
#include <faultline/fl_test.h>              // FL_TYPE_TEST_SETUP_CLEANUP, FL_SUITE_*
#include <faultline/fl_test_summary.h>      // FLTestSummary, init_faultline_test_summary
#include <faultline/fl_types.h>             // FL_TEST_FAILURE, FL_DISCOVERY_PHASE

enum {
    CLI_TEST_CAPTURE_MAX = 8192,
    CLI_TEST_LINE_MAX    = 128,
    CLI_TEST_PATH_MAX    = 64,
    CLI_TEST_JAN1_0900   = 1704099600, // 2024-01-01T09:00:00Z
    CLI_TEST_JAN2_1000   = 1704189600, // 2024-01-02T10:00:00Z
};

// Where every recorded fault happened, and how a report prints that location.
#define CLI_TEST_FAULT_FILE     "alloc.c"
#define CLI_TEST_FAULT_LINE     10
#define CLI_TEST_FAULT_LOCATION CLI_TEST_FAULT_FILE ":" FL_STR(CLI_TEST_FAULT_LINE)

/////////////////
//  Test Data  //
/////////////////

// The suite every case records. Static storage makes each address a constant, so the
// array and the suite can be initialized with them.
static FLTestCase  cli_alpha   = {.name = "alpha"};
static FLTestCase  cli_beta    = {.name = "beta"};
static FLTestCase *cli_cases[] = {&cli_alpha, &cli_beta};
static FLTestSuite cli_suite
    = {.name = "CliSuite", .count = FL_ARRAY_COUNT(cli_cases), .test_cases = cli_cases};

// One case's outcome in a run that cli_test_record builds.
typedef struct CliTestOutcome {
    FLResultCode code;
    i64          fault_sites;
    double       elapsed_seconds;
} CliTestOutcome;

// Both cases pass.
static CliTestOutcome const cli_both_pass[] = {{FL_PASS, 10, 1.0}, {FL_PASS, 10, 1.0}};

// beta fails.
static CliTestOutcome const cli_beta_fails[] = {{FL_PASS, 10, 1.0}, {FL_FAIL, 3, 1.0}};

// Both pass, and alpha exercises fewer fault sites than in cli_both_pass.
static CliTestOutcome const cli_alpha_drops[] = {{FL_PASS, 7, 1.0}, {FL_PASS, 10, 1.0}};

///////////////
//  Fixture  //
///////////////

typedef struct CliTest {
    FLTestCase   tc;
    FLCliSession session;
    Arena       *arena; // backs the contexts a case records
    char         db_path[CLI_TEST_PATH_MAX];
    char         out_path[CLI_TEST_PATH_MAX];
    char         err_path[CLI_TEST_PATH_MAX];
    char         line[CLI_TEST_LINE_MAX]; // wb_tokenize splits it in place
    char        *argv[WB_CLI_MAX_ARGS];
    char         capture[CLI_TEST_CAPTURE_MAX];
} CliTest;

static int cli_test_counter = 0;

// A scratch stream in the working directory, named for this case and `suffix`.
static FILE *cli_test_scratch(char *path, size_t size, char const *suffix) {
    FILE *file = NULL;
    snprintf(path, size, "faultline_cli_test_%d_%s.tmp", cli_test_counter, suffix);
    remove(path);
    if (fopen_s(&file, path, "w+b") != 0) {
        file = NULL;
    }
    FL_ASSERT_DETAILS(file != NULL, "cannot open the scratch file '%s'", path);
    return file;
}

static void cli_test_close(FILE **file, char *path) {
    if (*file != NULL) {
        fclose(*file);
        *file = NULL;
    }
    if (path[0] != '\0') {
        remove(path);
        path[0] = '\0';
    }
}

// Remove a database file and the sidecar files a journal or WAL connection leaves.
static void cli_test_remove_db(char const *path) {
    char sidecar[CLI_TEST_PATH_MAX + sizeof "-journal"];
    remove(path);
    snprintf(sidecar, sizeof sidecar, "%s-journal", path);
    remove(sidecar);
    snprintf(sidecar, sizeof sidecar, "%s-wal", path);
    remove(sidecar);
    snprintf(sidecar, sizeof sidecar, "%s-shm", path);
    remove(sidecar);
}

// The session gets this case's database before any command runs, so no command opens
// the file --db names by default.
static FL_SETUP_FN(setup_cli_cmd) {
    CliTest *t     = FL_CONTAINER_OF(tc, CliTest, tc);
    t->session     = (FLCliSession){0};
    t->out_path[0] = '\0';
    t->err_path[0] = '\0';

    cli_test_counter++;
    snprintf(t->db_path, sizeof t->db_path, "faultline_cli_test_%d.sqlite",
             cli_test_counter);
    cli_test_remove_db(t->db_path);

    t->arena          = new_arena(0, 0);
    t->session.db     = faultline_init_database(t->db_path);
    t->session.io.out = cli_test_scratch(t->out_path, sizeof t->out_path, "out");
    t->session.io.err = cli_test_scratch(t->err_path, sizeof t->err_path, "err");
}

static FL_CLEANUP_FN(cleanup_cli_cmd) {
    CliTest *t = FL_CONTAINER_OF(tc, CliTest, tc);
    faultline_cli_session_free(&t->session);
    cli_test_close(&t->session.io.out, t->out_path);
    cli_test_close(&t->session.io.err, t->err_path);
    if (t->arena != NULL) {
        release_arena(&t->arena);
        t->arena = NULL;
    }
    cli_test_remove_db(t->db_path);
}

///////////////
//  Helpers  //
///////////////

// Record one run of cli_suite that started at `started`, in which case i ends with
// outcomes[i]. A failing case records one fault at CLI_TEST_FAULT_LOCATION.
static i64 cli_test_record(CliTest *t, time_t started, CliTestOutcome const *outcomes) {
    FLContext fctx;
    faultline_initialize(&fctx, &cli_suite, t->arena);
    fctx.run_start_time = started;

    for (u32 i = 0; i < (u32)cli_suite.count; i++) {
        FLTestSummary *summary
            = faultline_test_summary_buffer_allocate_next_free_slot(&fctx.results);
        init_faultline_test_summary(summary, fctx.arena, i, outcomes[i].code, NULL, NULL);
        summary->faults_exercised = outcomes[i].fault_sites;
        summary->elapsed_seconds  = outcomes[i].elapsed_seconds;

        if (outcomes[i].code > FL_PASS) {
            summary->failure_type  = FL_TEST_FAILURE;
            summary->failure_phase = FL_DISCOVERY_PHASE;
            faultline_test_summary_add_fault(summary, 0, outcomes[i].code,
                                             FAULT_NO_RESOURCE, "boom", "",
                                             CLI_TEST_FAULT_FILE, CLI_TEST_FAULT_LINE);
        }
    }

    return faultline_run_record(t->session.db, &fctx);
}

// Run one command line as the executable does and return its status.
static int cli_test_run(CliTest *t, char const *text) {
    size_t const length = strlen(text);
    FL_ASSERT_TRUE(length < sizeof t->line);
    memcpy(t->line, text, length + 1);

    int const argc = wb_tokenize(t->line, t->argv, WB_CLI_MAX_ARGS);
    FL_ASSERT_TRUE(argc >= 0);
    return faultline_cli_run_once(&t->session, faultline_cli_root(), argc, t->argv);
}

// Everything the case's commands have written to `stream`.
static char const *cli_test_output(CliTest *t, FILE *stream) {
    fflush(stream);
    FL_ASSERT_EQ_INT(fseek(stream, 0, SEEK_SET), 0);

    // A short read is the end of a scratch file this fixture flushed a moment ago.
    // NOLINTNEXTLINE(clang-analyzer-unix.Errno)
    size_t const read = fread(t->capture, 1, sizeof t->capture - 1, stream);
    t->capture[read]  = '\0';

    // Back to the end, so the next command appends to what is there.
    FL_ASSERT_EQ_INT(fseek(stream, 0, SEEK_END), 0);
    return t->capture;
}

// The number of times `needle` occurs in `haystack`.
static int cli_test_count(char const *haystack, char const *needle) {
    size_t const length = strlen(needle);
    int          count  = 0;

    char const *at = strstr(haystack, needle);
    while (at != NULL) {
        count++;
        at = strstr(at + length, needle);
    }

    return count;
}

/////////////
//  Cases  //
/////////////

FL_TYPE_TEST_SETUP_CLEANUP("No arguments prints the root help", CliTest, cli_no_args_help,
                           setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, ""), WB_CLI_OK);

    char const *out = cli_test_output(t, t->session.io.out);
    FL_ASSERT_TRUE(strstr(out, "Usage:") != NULL);
    FL_ASSERT_TRUE(strstr(out, "Available Commands:") != NULL);
    FL_ASSERT_TRUE(strstr(out, "regression") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Result list shows runs newest first, up to its limit",
                           CliTest, cli_result_list, setup_cli_cmd, cleanup_cli_cmd) {
    cli_test_record(t, CLI_TEST_JAN1_0900, cli_both_pass);
    cli_test_record(t, CLI_TEST_JAN2_1000, cli_beta_fails);

    FL_ASSERT_EQ_INT(cli_test_run(t, "result list"), WB_CLI_OK);
    char const *out    = cli_test_output(t, t->session.io.out);
    char const *newest = strstr(out, "2024-01-02");
    char const *oldest = strstr(out, "2024-01-01");
    FL_ASSERT_TRUE(newest != NULL && oldest != NULL && newest < oldest);
    FL_ASSERT_EQ_INT(cli_test_count(out, "CliSuite"), 2);

    // One more row across both commands' output.
    FL_ASSERT_EQ_INT(cli_test_run(t, "result list -n 1"), WB_CLI_OK);
    FL_ASSERT_EQ_INT(cli_test_count(cli_test_output(t, t->session.io.out), "CliSuite"),
                     3);
}

FL_TYPE_TEST_SETUP_CLEANUP("Result show prints each case's result", CliTest,
                           cli_result_show, setup_cli_cmd, cleanup_cli_cmd) {
    i64 const run_id = cli_test_record(t, CLI_TEST_JAN1_0900, cli_beta_fails);

    char line[CLI_TEST_LINE_MAX];
    snprintf(line, sizeof line, "result show %lld", (long long)run_id);
    FL_ASSERT_EQ_INT(cli_test_run(t, line), WB_CLI_OK);

    char const *out = cli_test_output(t, t->session.io.out);
    FL_ASSERT_TRUE(strstr(out, "alpha") != NULL);
    FL_ASSERT_TRUE(strstr(out, "beta") != NULL);
    FL_ASSERT_TRUE(strstr(out, CLI_TEST_FAULT_LOCATION) != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Result show refuses an id that is not a number", CliTest,
                           cli_result_show_bad_id, setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "result show abc"), WB_CLI_USAGE);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.err),
                          "\"abc\" is not a run id")
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Result show reports a run that does not exist", CliTest,
                           cli_result_show_missing, setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "result show 999"), WB_CLI_ERROR);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.err), "no run has id 999")
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Failure list shows the latest run's faults", CliTest,
                           cli_failure_list, setup_cli_cmd, cleanup_cli_cmd) {
    cli_test_record(t, CLI_TEST_JAN1_0900, cli_beta_fails);

    FL_ASSERT_EQ_INT(cli_test_run(t, "failure list"), WB_CLI_OK);
    char const *out = cli_test_output(t, t->session.io.out);
    FL_ASSERT_TRUE(strstr(out, "beta") != NULL);
    FL_ASSERT_TRUE(strstr(out, CLI_TEST_FAULT_LOCATION) != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Trend list shows each day with runs", CliTest, cli_trend_list,
                           setup_cli_cmd, cleanup_cli_cmd) {
    cli_test_record(t, CLI_TEST_JAN1_0900, cli_both_pass);
    cli_test_record(t, CLI_TEST_JAN2_1000, cli_beta_fails);

    FL_ASSERT_EQ_INT(cli_test_run(t, "trend list"), WB_CLI_OK);
    char const *out = cli_test_output(t, t->session.io.out);
    FL_ASSERT_TRUE(strstr(out, "2024-01-01") != NULL);
    FL_ASSERT_TRUE(strstr(out, "2024-01-02") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Suite list names the suite, and suite show adds its cases",
                           CliTest, cli_suite_list_show, setup_cli_cmd, cleanup_cli_cmd) {
    cli_test_record(t, CLI_TEST_JAN1_0900, cli_both_pass);

    FL_ASSERT_EQ_INT(cli_test_run(t, "suite list"), WB_CLI_OK);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.out), "CliSuite") != NULL);

    FL_ASSERT_EQ_INT(cli_test_run(t, "suite show CliSuite -v"), WB_CLI_OK);
    char const *out = cli_test_output(t, t->session.io.out);
    FL_ASSERT_TRUE(strstr(out, "=== Suite Summary: CliSuite ===") != NULL);
    FL_ASSERT_TRUE(strstr(out, "=== Test Cases ===") != NULL);
    FL_ASSERT_TRUE(strstr(out, "alpha") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Suite show reports a suite that does not exist", CliTest,
                           cli_suite_show_missing, setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "suite show NoSuchSuite"), WB_CLI_ERROR);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.err),
                          "no suite is named \"NoSuchSuite\"")
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Case list shows each case", CliTest, cli_case_list,
                           setup_cli_cmd, cleanup_cli_cmd) {
    cli_test_record(t, CLI_TEST_JAN1_0900, cli_beta_fails);

    FL_ASSERT_EQ_INT(cli_test_run(t, "case list -v"), WB_CLI_OK);
    char const *out = cli_test_output(t, t->session.io.out);
    FL_ASSERT_TRUE(strstr(out, "alpha") != NULL);
    FL_ASSERT_TRUE(strstr(out, "beta") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Baseline reset clears a regression", CliTest,
                           cli_baseline_reset, setup_cli_cmd, cleanup_cli_cmd) {
    cli_test_record(t, CLI_TEST_JAN1_0900, cli_both_pass);
    cli_test_record(t, CLI_TEST_JAN2_1000, cli_alpha_drops);

    FL_ASSERT_EQ_INT(cli_test_run(t, "regression list"), WB_CLI_OK);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.out), "COVERAGE") != NULL);

    FL_ASSERT_EQ_INT(cli_test_run(t, "baseline reset -s CliSuite -c alpha"), WB_CLI_OK);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.out), "Reset 1 baseline to")
                   != NULL);

    // Only the list after the reset finds nothing.
    FL_ASSERT_EQ_INT(cli_test_run(t, "regression list"), WB_CLI_OK);
    FL_ASSERT_EQ_INT(cli_test_count(cli_test_output(t, t->session.io.out),
                                    "(no regressions)"),
                     1);
}

FL_TYPE_TEST_SETUP_CLEANUP("Hotspot list names the fault's location", CliTest,
                           cli_hotspot_list, setup_cli_cmd, cleanup_cli_cmd) {
    cli_test_record(t, CLI_TEST_JAN1_0900, cli_beta_fails);

    FL_ASSERT_EQ_INT(cli_test_run(t, "hotspot list"), WB_CLI_OK);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.out), CLI_TEST_FAULT_LOCATION)
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("A report refuses to run with --no-db", CliTest,
                           cli_no_db_refused, setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "result list --no-db"), WB_CLI_ERROR);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.err),
                          "\"faultline result list\" needs the results database")
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("An undeclared flag is a usage error", CliTest,
                           cli_unknown_flag, setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "result list --format json"), WB_CLI_USAGE);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.err),
                          "unknown flag \"--format\"")
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("show is not a command", CliTest, cli_show_unknown,
                           setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "show results"), WB_CLI_USAGE);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.err),
                          "unknown command \"show\"")
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("help prints a command's usage line", CliTest,
                           cli_help_command, setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "help suite show"), WB_CLI_OK);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.out),
                          "faultline suite show <name> [flags]")
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("run needs at least one suite", CliTest, cli_run_needs_suite,
                           setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "run"), WB_CLI_USAGE);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.err),
                          "needs at least 1 argument")
                   != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("version prints the version", CliTest, cli_version,
                           setup_cli_cmd, cleanup_cli_cmd) {
    FL_ASSERT_EQ_INT(cli_test_run(t, "version"), WB_CLI_OK);
    FL_ASSERT_TRUE(strstr(cli_test_output(t, t->session.io.out), "FaultLine version")
                   != NULL);
}

//////////////////////////
//  Suite Registration  //
//////////////////////////

FL_SUITE_BEGIN(FaultlineCli)
FL_SUITE_ADD_EMBEDDED(cli_no_args_help)
FL_SUITE_ADD_EMBEDDED(cli_result_list)
FL_SUITE_ADD_EMBEDDED(cli_result_show)
FL_SUITE_ADD_EMBEDDED(cli_result_show_bad_id)
FL_SUITE_ADD_EMBEDDED(cli_result_show_missing)
FL_SUITE_ADD_EMBEDDED(cli_failure_list)
FL_SUITE_ADD_EMBEDDED(cli_trend_list)
FL_SUITE_ADD_EMBEDDED(cli_suite_list_show)
FL_SUITE_ADD_EMBEDDED(cli_suite_show_missing)
FL_SUITE_ADD_EMBEDDED(cli_case_list)
FL_SUITE_ADD_EMBEDDED(cli_baseline_reset)
FL_SUITE_ADD_EMBEDDED(cli_hotspot_list)
FL_SUITE_ADD_EMBEDDED(cli_no_db_refused)
FL_SUITE_ADD_EMBEDDED(cli_unknown_flag)
FL_SUITE_ADD_EMBEDDED(cli_show_unknown)
FL_SUITE_ADD_EMBEDDED(cli_help_command)
FL_SUITE_ADD_EMBEDDED(cli_run_needs_suite)
FL_SUITE_ADD_EMBEDDED(cli_version)
FL_SUITE_END;

FL_GET_TEST_SUITE("Faultline CLI", FaultlineCli)
