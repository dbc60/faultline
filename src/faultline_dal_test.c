/**
 * @file faultline_dal_test.c
 * @author Douglas Cuthbertson
 * @brief Test cases for the data-access layer.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Each case builds FLContexts by hand, records them through the DAL into a private
 * database file, and checks the rows with raw SQL or through the DAL's reads. Cleanup
 * runs even when the body throws, so a failed assertion still closes the connection,
 * releases the arena, and removes the file.
 */
#include <faultline_dal.h>
#include <faultline_sqlite.h>

#include "faultline_dal_test.h"             // TestDal
#include <faultline/arena.h>                // new_arena, release_arena
#include <faultline/fault.h>                // FAULT_NO_RESOURCE
#include <faultline/fl_abbreviated_types.h> // i64, u32
#include <faultline/fl_context.h>           // FLContext, faultline_initialize
#include <faultline/fl_exception_assert.h>  // FL_ASSERT_*, FL_FAIL
#include <faultline/fl_macros.h>            // FL_ARRAY_COUNT, FL_CONTAINER_OF
#include <faultline/fl_result_codes.h>      // FLResultCode
#include <faultline/fl_test.h>              // FL_TYPE_TEST_SETUP_CLEANUP, FLTestSuite
#include <faultline/fl_test_summary.h>      // FLTestSummary, init_faultline_test_summary
#include <faultline/fl_try.h>               // FL_TRY, FL_CATCH, FL_END_TRY
#include <faultline/fl_types.h>             // FLFailureType, FLTestPhase
#include <sqlite/sqlite3.h>

#include <stdbool.h> // bool
#include <stdint.h>  // uintptr_t
#include <stdio.h>   // snprintf, remove
#include <string.h>  // strcmp
#include <time.h>    // time_t

enum {
    DAL_TEST_START_TIME = 86400,      // 1970-01-02T00:00:00Z as a time_t
    DAL_TEST_RESOURCE   = 0x1234,     // the address an invalid-free fault reports
    DAL_TEST_JAN1_0900  = 1704099600, // 2024-01-01T09:00:00Z
    DAL_TEST_JAN1_1500  = 1704121200, // 2024-01-01T15:00:00Z
    DAL_TEST_JAN2_1000  = 1704189600, // 2024-01-02T10:00:00Z
    DAL_TEST_JAN3_1000  = 1704276000, // 2024-01-03T10:00:00Z
};

// Runtime growth, in percent, that the regression tests treat as a regression.
static double const dal_test_threshold_pct = 20.0;

/////////////////
//  Test Data  //
/////////////////

// The cases and suites the tests record. Static storage makes each address a constant,
// so the arrays and suites below can be initialized with them.
static FLTestCase dal_alpha    = {.name = "alpha"};
static FLTestCase dal_beta     = {.name = "beta"};
static FLTestCase dal_twin     = {.name = "twin"};
static FLTestCase dal_twin_too = {.name = "twin"};
static FLTestCase dal_pass     = {.name = "pass"};
static FLTestCase dal_leak     = {.name = "leak"};
static FLTestCase dal_bad_free = {.name = "bad_free"};
static FLTestCase dal_setup    = {.name = "setup"};
static FLTestCase dal_cleanup  = {.name = "cleanup"};
static FLTestCase dal_cov_only = {.name = "cov_only"};
static FLTestCase dal_rt_only  = {.name = "rt_only"};
static FLTestCase dal_both     = {.name = "both"};
static FLTestCase dal_clean    = {.name = "clean"};
static FLTestCase dal_cov_up   = {.name = "cov_up"};

static FLTestCase *dal_alpha_only[] = {&dal_alpha};
static FLTestCase *dal_alpha_beta[] = {&dal_alpha, &dal_beta};
static FLTestCase *dal_twins[]      = {&dal_twin, &dal_twin_too};
static FLTestCase *dal_outcomes[]
    = {&dal_pass, &dal_leak, &dal_bad_free, &dal_setup, &dal_cleanup};
static FLTestCase *dal_classes[]
    = {&dal_cov_only, &dal_rt_only, &dal_both, &dal_clean, &dal_cov_up};

static FLTestSuite dal_suite_alpha = {
    .name = "DalSuite", .count = FL_ARRAY_COUNT(dal_alpha_only), .test_cases = dal_alpha_only};
static FLTestSuite dal_suite_alpha_beta = {
    .name = "DalSuite", .count = FL_ARRAY_COUNT(dal_alpha_beta), .test_cases = dal_alpha_beta};
static FLTestSuite dal_suite_other = {
    .name = "OtherSuite", .count = FL_ARRAY_COUNT(dal_alpha_only), .test_cases = dal_alpha_only};
static FLTestSuite dal_suite_twins = {
    .name = "DupSuite", .count = FL_ARRAY_COUNT(dal_twins), .test_cases = dal_twins};
static FLTestSuite dal_suite_outcomes = {
    .name = "OutcomeSuite", .count = FL_ARRAY_COUNT(dal_outcomes), .test_cases = dal_outcomes};
static FLTestSuite dal_suite_classes = {
    .name = "ClassSuite", .count = FL_ARRAY_COUNT(dal_classes), .test_cases = dal_classes};

// One result row as text, selected by case name. quote() spells a NULL column as NULL.
static char const *const dal_result_sql
    = "SELECT x.case_index || '|' || x.result_code || '|' || x.failure_type || '|'"
      " || quote(x.failure_phase) || '|' || quote(x.reason) || '|' || quote(x.details)"
      " || '|' || x.fault_sites"
      " FROM results x JOIN cases c ON c.id = x.case_id WHERE c.name = '%s';";

///////////////
//  Fixture  //
///////////////

static int dal_test_counter = 0;

// Remove a case's database file and the sidecar files a journal or WAL connection leaves.
static void dal_test_remove(TestDal const *t) {
    char sidecar[sizeof t->path + sizeof "-journal"];
    remove(t->path);
    snprintf(sidecar, sizeof sidecar, "%s-journal", t->path);
    remove(sidecar);
    snprintf(sidecar, sizeof sidecar, "%s-wal", t->path);
    remove(sidecar);
    snprintf(sidecar, sizeof sidecar, "%s-shm", t->path);
    remove(sidecar);
}

// A fresh path per case. The counter also advances across fault-injection re-runs of a
// case, so a re-run never opens the previous pass's file.
static FL_SETUP_FN(setup_dal_db) {
    TestDal *t = FL_CONTAINER_OF(tc, TestDal, tc);
    t->db      = NULL;
    t->arena   = NULL;
    snprintf(t->path, sizeof t->path, "faultline_dal_test_%d.sqlite", ++dal_test_counter);
    dal_test_remove(t);
    t->arena = new_arena(0, 0);
}

static FL_CLEANUP_FN(cleanup_dal_db) {
    TestDal *t = FL_CONTAINER_OF(tc, TestDal, tc);
    if (t->db != NULL) {
        sqlite3_close_v2(t->db);
        t->db = NULL;
    }
    if (t->arena != NULL) {
        release_arena(&t->arena);
        t->arena = NULL;
    }
    dal_test_remove(t);
}

///////////////
//  Helpers  //
///////////////

// Run SQL that must succeed.
static void dal_test_exec(sqlite3 *db, char const *sql) {
    char *errmsg = NULL;
    int   rc     = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        char details[320];
        snprintf(details, sizeof details, "%s in \"%.160s\"",
                 errmsg ? errmsg : sqlite3_errstr(rc), sql);
        sqlite3_free(errmsg); // free before the throw
        FL_FAIL("%s", details);
    }
}

// First column of the first row as an int, or fallback when the query yields no row.
static int dal_test_query_int(sqlite3 *db, char const *sql, int fallback) {
    sqlite3_stmt *stmt  = NULL;
    int           value = fallback;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW) {
        value = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return value;
}

// First column of the first row as text, or "" when the query yields no row or NULL.
static void dal_test_query_text(sqlite3 *db, char const *sql, char *out, size_t size) {
    sqlite3_stmt *stmt = NULL;
    out[0]             = '\0';
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW) {
        char const *text = (char const *)sqlite3_column_text(stmt, 0);
        if (text != NULL) {
            snprintf(out, size, "%s", text);
        }
    }
    sqlite3_finalize(stmt);
}

// Copy a borrowed row string into a collector, spelling NULL as "".
static void dal_test_copy(char *out, size_t size, char const *text) {
    snprintf(out, size, "%s", (text != NULL) ? text : "");
}

// Add a summary for the case at `index` to the context's results.
static FLTestSummary *dal_test_add_summary(FLContext *fctx, u32 index, FLResultCode code) {
    FLTestSummary *summary
        = faultline_test_summary_buffer_allocate_next_free_slot(&fctx->results);
    init_faultline_test_summary(summary, fctx->arena, index, code, NULL, NULL);
    return summary;
}

// Add a failed summary, with the kind of failure and the phase it happened in.
static FLTestSummary *dal_test_add_failure(FLContext *fctx, u32 index, FLResultCode code,
                                           FLFailureType type, FLTestPhase phase) {
    FLTestSummary *summary = dal_test_add_summary(fctx, index, code);
    summary->failure_type  = type;
    summary->failure_phase = phase;
    return summary;
}

// One case's outcome in a run that dal_test_record builds.
typedef struct DalTestOutcome {
    FLResultCode code;
    i64          fault_sites;
    double       elapsed_seconds;
} DalTestOutcome;

// Record one run of `suite` started at `started`, in which case i ends with outcomes[i].
// A failing outcome is recorded as a test-body failure found in discovery.
static i64 dal_test_record(TestDal *t, FLTestSuite *suite, time_t started,
                           DalTestOutcome const *outcomes) {
    FLContext fctx;
    faultline_initialize(&fctx, suite, t->arena);
    fctx.run_start_time = started;

    for (u32 i = 0; i < (u32)suite->count; i++) {
        FLTestSummary *summary
            = (outcomes[i].code > FL_PASS)
                  ? dal_test_add_failure(&fctx, i, outcomes[i].code, FL_TEST_FAILURE,
                                         FL_DISCOVERY_PHASE)
                  : dal_test_add_summary(&fctx, i, outcomes[i].code);
        summary->faults_exercised = outcomes[i].fault_sites;
        summary->elapsed_seconds  = outcomes[i].elapsed_seconds;
    }

    return faultline_run_record(t->db, &fctx);
}

////////////////////////////////
//  Collectors for the reads  //
////////////////////////////////

typedef struct DalRunCollector {
    int  count;
    int  stop_after; // end the walk after this many rows; 0 visits every row
    i64  ids[8];
    char suites[8][32];
    i64  cases_run[8];
    i64  passed[8];
} DalRunCollector;

static bool dal_collect_run(FLRunRow const *row, void *user) {
    DalRunCollector *c = (DalRunCollector *)user;
    if (c->count < (int)FL_ARRAY_COUNT(c->ids)) {
        c->ids[c->count] = row->id;
        dal_test_copy(c->suites[c->count], sizeof c->suites[c->count], row->suite_name);
        c->cases_run[c->count] = row->cases_run;
        c->passed[c->count]    = row->passed;
    }
    c->count++;
    return c->stop_after == 0 || c->count < c->stop_after;
}

typedef struct DalResultCollector {
    int  count;
    char names[4][32];
    int  codes[4];
    bool has_file[4];
    char files[4][32];
    int  lines[4];
} DalResultCollector;

static bool dal_collect_result(FLResultRow const *row, void *user) {
    DalResultCollector *c = (DalResultCollector *)user;
    if (c->count < (int)FL_ARRAY_COUNT(c->names)) {
        dal_test_copy(c->names[c->count], sizeof c->names[c->count], row->case_name);
        c->codes[c->count]    = row->result_code;
        c->has_file[c->count] = (row->source_file != NULL);
        dal_test_copy(c->files[c->count], sizeof c->files[c->count], row->source_file);
        c->lines[c->count] = row->source_line;
    }
    c->count++;
    return true;
}

typedef struct DalFailureCollector {
    int  count;
    i64  run_ids[4];
    char files[4][32];
    i64  resources[4];
} DalFailureCollector;

static bool dal_collect_failure(FLFailureRow const *row, void *user) {
    DalFailureCollector *c = (DalFailureCollector *)user;
    if (c->count < (int)FL_ARRAY_COUNT(c->run_ids)) {
        c->run_ids[c->count] = row->run_id;
        dal_test_copy(c->files[c->count], sizeof c->files[c->count], row->source_file);
        c->resources[c->count] = row->resource_address;
    }
    c->count++;
    return true;
}

typedef struct DalSuiteCollector {
    int    count;
    char   names[4][32];
    i64    runs[4];
    i64    cases[4];
    char   first_runs[4][32];
    char   last_runs[4][32];
    double pass_rates[4];
    i64    fault_sites[4];
    i64    failures[4];
} DalSuiteCollector;

static bool dal_collect_suite(FLSuiteRow const *row, void *user) {
    DalSuiteCollector *c = (DalSuiteCollector *)user;
    if (c->count < (int)FL_ARRAY_COUNT(c->names)) {
        dal_test_copy(c->names[c->count], sizeof c->names[c->count], row->name);
        c->runs[c->count]  = row->runs;
        c->cases[c->count] = row->cases;
        dal_test_copy(c->first_runs[c->count], sizeof c->first_runs[c->count],
                      row->first_run_at);
        dal_test_copy(c->last_runs[c->count], sizeof c->last_runs[c->count],
                      row->last_run_at);
        c->pass_rates[c->count]  = row->pass_rate;
        c->fault_sites[c->count] = row->fault_sites;
        c->failures[c->count]    = row->failures;
    }
    c->count++;
    return true;
}

typedef struct DalCaseCollector {
    int  count;
    i64  appearances[4];
    i64  failures[4];
    bool has_baseline[4];
    i64  baseline_sites[4];
    i64  last_sites[4];
} DalCaseCollector;

static bool dal_collect_case(FLCaseRow const *row, void *user) {
    DalCaseCollector *c = (DalCaseCollector *)user;
    if (c->count < (int)FL_ARRAY_COUNT(c->appearances)) {
        c->appearances[c->count]    = row->appearances;
        c->failures[c->count]       = row->failures;
        c->has_baseline[c->count]   = row->has_baseline;
        c->baseline_sites[c->count] = row->baseline_fault_sites;
        c->last_sites[c->count]     = row->last_fault_sites;
    }
    c->count++;
    return true;
}

typedef struct DalRegressionCollector {
    int  count;
    char names[8][32];
    bool coverage[8];
    bool runtime[8];
    i64  baseline_sites[8];
    i64  last_sites[8];
} DalRegressionCollector;

static bool dal_collect_regression(FLRegressionRow const *row, void *user) {
    DalRegressionCollector *c = (DalRegressionCollector *)user;
    if (c->count < (int)FL_ARRAY_COUNT(c->names)) {
        dal_test_copy(c->names[c->count], sizeof c->names[c->count], row->case_name);
        c->coverage[c->count]       = row->coverage_regression;
        c->runtime[c->count]        = row->runtime_regression;
        c->baseline_sites[c->count] = row->baseline_fault_sites;
        c->last_sites[c->count]     = row->last_fault_sites;
    }
    c->count++;
    return true;
}

// The position of a case in a regression collector, or -1 when it was not visited.
static int dal_regression_index(DalRegressionCollector const *c, char const *name) {
    for (int i = 0; i < c->count && i < (int)FL_ARRAY_COUNT(c->names); i++) {
        if (strcmp(c->names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

// Pass rates are kept as integer per-mille so the tests compare them without a tolerance.
typedef struct DalTrendCollector {
    int  count;
    char days[4][16];
    int  runs[4];
    int  pass_milli[4];
    int  failures[4];
    bool has_previous[4];
    int  previous_pass_milli[4];
    int  previous_failures[4];
} DalTrendCollector;

static bool dal_collect_trend(FLTrendRow const *row, void *user) {
    DalTrendCollector *c = (DalTrendCollector *)user;
    if (c->count < (int)FL_ARRAY_COUNT(c->days)) {
        dal_test_copy(c->days[c->count], sizeof c->days[c->count], row->day);
        c->runs[c->count]                = (int)row->runs;
        c->pass_milli[c->count]          = (int)(row->pass_rate * 1000.0 + 0.5);
        c->failures[c->count]            = (int)row->failures;
        c->has_previous[c->count]        = row->has_previous;
        c->previous_pass_milli[c->count] = (int)(row->previous_pass_rate * 1000.0 + 0.5);
        c->previous_failures[c->count]   = (int)row->previous_failures;
    }
    c->count++;
    return true;
}

typedef struct DalHotspotCollector {
    int  count;
    char files[4][32];
    int  lines[4];
    i64  faults[4];
    i64  cases[4];
} DalHotspotCollector;

static bool dal_collect_hotspot(FLHotspotRow const *row, void *user) {
    DalHotspotCollector *c = (DalHotspotCollector *)user;
    if (c->count < (int)FL_ARRAY_COUNT(c->files)) {
        dal_test_copy(c->files[c->count], sizeof c->files[c->count], row->source_file);
        c->lines[c->count]  = row->source_line;
        c->faults[c->count] = row->faults;
        c->cases[c->count]  = row->cases;
    }
    c->count++;
    return true;
}

//////////////////
//  Write Path  //
//////////////////

// Recording writes the suite, the run, each case, its result, and its faults, and
// stores each absent value as NULL.
FL_TYPE_TEST_SETUP_CLEANUP("Record Round Trips", TestDal, dal_record_round_trips,
                           setup_dal_db, cleanup_dal_db) {
    FLContext fctx;
    faultline_initialize(&fctx, &dal_suite_alpha_beta, t->arena);
    fctx.run_start_time = DAL_TEST_START_TIME;

    dal_test_add_summary(&fctx, 0, FL_PASS)->faults_exercised = 3;

    FLTestSummary *fail
        = dal_test_add_failure(&fctx, 1, FL_FAIL, FL_TEST_FAILURE, FL_INJECTION_PHASE);
    fail->reason           = "boom";
    fail->details          = "bad state";
    fail->faults_exercised = 2;
    faultline_test_summary_add_fault(fail, 0, FL_FAIL, FAULT_NO_RESOURCE, "boom", "",
                                     "alloc.c", 10);
    faultline_test_summary_add_fault(fail, 2, FL_INVALID_FREE,
                                     (void const *)(uintptr_t)DAL_TEST_RESOURCE,
                                     "invalid free", "", "free.c", 20);

    t->db            = faultline_init_database(t->path);
    i64 const run_id = faultline_run_record(t->db, &fctx);
    FL_ASSERT_GT((int)run_id, 0);

    char sql[512];
    char actual[160];
    char expected[160];

    // The run names its suite, keeps its start time, and counts the suite's cases.
    dal_test_query_text(t->db,
                        "SELECT s.name || '|' || r.started_at || '|' || r.case_count"
                        " || '|' || (r.completed_at IS NOT NULL)"
                        " FROM runs r JOIN suites s ON s.id = r.suite_id;",
                        actual, sizeof actual);
    FL_ASSERT_STR_EQ("DalSuite|1970-01-02T00:00:00Z|2|1", actual);

    // The passing result has no phase, reason, or details.
    snprintf(sql, sizeof sql, dal_result_sql, "alpha");
    dal_test_query_text(t->db, sql, actual, sizeof actual);
    snprintf(expected, sizeof expected, "0|%d|%d|NULL|NULL|NULL|3", FL_PASS,
             FL_FAILURE_NONE);
    FL_ASSERT_STR_EQ(expected, actual);

    snprintf(sql, sizeof sql, dal_result_sql, "beta");
    dal_test_query_text(t->db, sql, actual, sizeof actual);
    snprintf(expected, sizeof expected, "1|%d|%d|%d|'boom'|'bad state'|2", FL_FAIL,
             FL_TEST_FAILURE, FL_INJECTION_PHASE);
    FL_ASSERT_STR_EQ(expected, actual);

    // Faults keep their order. Empty details and a missing resource are NULL.
    dal_test_query_text(t->db,
                        "SELECT group_concat(position || ':' || fault_index || ':'"
                        " || result_code || ':' || quote(details) || ':'"
                        " || quote(source_file) || ':' || quote(source_line) || ':'"
                        " || quote(resource_address), ',' ORDER BY position)"
                        " FROM faults;",
                        actual, sizeof actual);
    snprintf(expected, sizeof expected,
             "0:0:%d:NULL:'alloc.c':10:NULL,1:2:%d:NULL:'free.c':20:%d", FL_FAIL,
             FL_INVALID_FREE, DAL_TEST_RESOURCE);
    FL_ASSERT_STR_EQ(expected, actual);
}

// A suite with two cases of the same name cannot be recorded, and nothing of the run
// remains: no suite, run, case, or result.
FL_TYPE_TEST_SETUP_CLEANUP("Duplicate Case Name Rolls Back", TestDal,
                           dal_record_refuses_duplicates, setup_dal_db, cleanup_dal_db) {
    FLContext fctx;
    faultline_initialize(&fctx, &dal_suite_twins, t->arena);
    dal_test_add_summary(&fctx, 0, FL_PASS);
    dal_test_add_summary(&fctx, 1, FL_PASS);

    t->db = faultline_init_database(t->path);

    bool caught = false;
    FL_TRY {
        faultline_run_record(t->db, &fctx);
    }
    FL_CATCH(faultline_dal_write_failed) {
        caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(caught);
    FL_ASSERT_EQ(0, dal_test_query_int(t->db,
                                           "SELECT (SELECT count(*) FROM suites)"
                                           " + (SELECT count(*) FROM runs)"
                                           " + (SELECT count(*) FROM cases)"
                                           " + (SELECT count(*) FROM results);",
                                           -1));
}

// run_totals counts each outcome from the recorded results.
FL_TYPE_TEST_SETUP_CLEANUP("Run Totals Match Results", TestDal, dal_run_totals_match,
                           setup_dal_db, cleanup_dal_db) {
    FLContext fctx;
    faultline_initialize(&fctx, &dal_suite_outcomes, t->arena);

    dal_test_add_summary(&fctx, 0, FL_PASS)->faults_exercised = 1;
    dal_test_add_failure(&fctx, 1, FL_LEAK, FL_LEAK_FAILURE, FL_INJECTION_PHASE)
        ->faults_exercised = 2;
    dal_test_add_failure(&fctx, 2, FL_INVALID_FREE, FL_INVALID_FREE_FAILURE,
                         FL_INJECTION_PHASE)
        ->faults_exercised = 3;
    dal_test_add_failure(&fctx, 3, FL_FAIL, FL_SETUP_FAILURE, FL_DISCOVERY_PHASE)
        ->faults_exercised = 4;
    dal_test_add_failure(&fctx, 4, FL_FAIL, FL_CLEANUP_FAILURE, FL_DISCOVERY_PHASE)
        ->faults_exercised = 5;

    t->db            = faultline_init_database(t->path);
    i64 const run_id = faultline_run_record(t->db, &fctx);

    char sql[512];
    snprintf(sql, sizeof sql,
             "SELECT cases_run || '|' || passed || '|' || passed_with_leaks || '|'"
             " || failed || '|' || setup_failures || '|' || cleanup_failures || '|'"
             " || invalid_frees || '|' || fault_sites || '|' || round(pass_rate, 3)"
             " FROM run_totals WHERE run_id = %lld;",
             (long long)run_id);

    char actual[64];
    dal_test_query_text(t->db, sql, actual, sizeof actual);

    // 5 cases: 1 passed, 1 leaked, 3 failed (one of them an invalid free, one in setup,
    // one in cleanup), 15 fault sites, and a pass rate of 1 in 5.
    FL_ASSERT_STR_EQ("5|1|1|3|1|1|1|15|0.2", actual);
}

// A case's baseline is the run that first recorded it, and later runs leave it alone. A
// context whose start time was never stamped records the current time as the start.
FL_TYPE_TEST_SETUP_CLEANUP("Baseline Set On First Sighting", TestDal,
                           dal_baseline_first_sighting, setup_dal_db, cleanup_dal_db) {
    t->db = faultline_init_database(t->path);

    FLContext fctx;
    faultline_initialize(&fctx, &dal_suite_alpha, t->arena);
    dal_test_add_summary(&fctx, 0, FL_PASS);
    i64 const first = faultline_run_record(t->db, &fctx);

    faultline_initialize(&fctx, &dal_suite_alpha_beta, t->arena);
    dal_test_add_summary(&fctx, 0, FL_PASS);
    dal_test_add_summary(&fctx, 1, FL_PASS);
    i64 const second = faultline_run_record(t->db, &fctx);

    FL_ASSERT_GT((int)second, (int)first);
    FL_ASSERT_EQ((int)first, dal_test_query_int(t->db,
                                                    "SELECT baseline_run_id FROM cases"
                                                    " WHERE name = 'alpha';",
                                                    -1));
    FL_ASSERT_EQ((int)second, dal_test_query_int(t->db,
                                                     "SELECT baseline_run_id FROM cases"
                                                     " WHERE name = 'beta';",
                                                     -1));
    FL_ASSERT_EQ(2, dal_test_query_int(t->db,
                                           "SELECT count(*) FROM cases"
                                           " WHERE baseline_set_at IS NOT NULL;",
                                           -1));
    FL_ASSERT_EQ(1, dal_test_query_int(t->db, "SELECT count(*) FROM suites;", -1));
    FL_ASSERT_EQ(0, dal_test_query_int(t->db,
                                           "SELECT count(*) FROM runs"
                                           " WHERE started_at < '2000';",
                                           -1));
}

// Deleting a run removes its results and faults, and clears the baselines that name it.
FL_TYPE_TEST_SETUP_CLEANUP("Deleting A Run Cascades", TestDal, dal_run_delete_cascades,
                           setup_dal_db, cleanup_dal_db) {
    FLContext fctx;
    faultline_initialize(&fctx, &dal_suite_alpha, t->arena);
    FLTestSummary *fail
        = dal_test_add_failure(&fctx, 0, FL_FAIL, FL_TEST_FAILURE, FL_DISCOVERY_PHASE);
    faultline_test_summary_add_fault(fail, 0, FL_FAIL, FAULT_NO_RESOURCE, "boom", "",
                                     "alloc.c", 10);

    t->db            = faultline_init_database(t->path);
    i64 const run_id = faultline_run_record(t->db, &fctx);
    FL_ASSERT_EQ(1, dal_test_query_int(t->db, "SELECT count(*) FROM faults;", -1));

    char sql[64];
    snprintf(sql, sizeof sql, "DELETE FROM runs WHERE id = %lld;", (long long)run_id);
    dal_test_exec(t->db, sql);

    FL_ASSERT_EQ(0, dal_test_query_int(t->db, "SELECT count(*) FROM results;", -1));
    FL_ASSERT_EQ(0, dal_test_query_int(t->db, "SELECT count(*) FROM faults;", -1));
    FL_ASSERT_EQ(1, dal_test_query_int(t->db,
                                           "SELECT count(*) FROM cases"
                                           " WHERE baseline_run_id IS NULL;",
                                           -1));
}

/////////////////
//  Read Path  //
/////////////////

// Runs come newest first. The suite filter, the limit, and a visitor that returns false
// each end the walk early.
FL_TYPE_TEST_SETUP_CLEANUP("Run List Newest First", TestDal, dal_run_list_newest_first,
                           setup_dal_db, cleanup_dal_db) {
    DalTestOutcome const one_pass[]  = {{FL_PASS, 1, 0.1}};
    DalTestOutcome const pass_fail[] = {{FL_PASS, 1, 0.1}, {FL_FAIL, 2, 0.2}};

    t->db = faultline_init_database(t->path);

    i64 const first  = dal_test_record(t, &dal_suite_alpha, DAL_TEST_START_TIME, one_pass);
    i64 const second = dal_test_record(t, &dal_suite_other, DAL_TEST_START_TIME, one_pass);
    i64 const third
        = dal_test_record(t, &dal_suite_alpha_beta, DAL_TEST_START_TIME, pass_fail);

    DalRunCollector all = {0};
    faultline_run_list(t->db, NULL, 0, dal_collect_run, &all);
    FL_ASSERT_EQ(3, all.count);
    FL_ASSERT_EQ((int)third, (int)all.ids[0]);
    FL_ASSERT_EQ((int)second, (int)all.ids[1]);
    FL_ASSERT_EQ((int)first, (int)all.ids[2]);
    FL_ASSERT_STR_EQ("OtherSuite", all.suites[1]);
    FL_ASSERT_EQ(2, (int)all.cases_run[0]);
    FL_ASSERT_EQ(1, (int)all.passed[0]);

    DalRunCollector suite_only = {0};
    faultline_run_list(t->db, "DalSuite", 0, dal_collect_run, &suite_only);
    FL_ASSERT_EQ(2, suite_only.count);

    DalRunCollector limited = {0};
    faultline_run_list(t->db, NULL, 2, dal_collect_run, &limited);
    FL_ASSERT_EQ(2, limited.count);

    DalRunCollector stopped = {.stop_after = 1};
    faultline_run_list(t->db, NULL, 0, dal_collect_run, &stopped);
    FL_ASSERT_EQ(1, stopped.count);
}

// Finding a run visits it once, and an unknown id visits nothing. The run's results come
// in case order, each with the location of its first fault.
FL_TYPE_TEST_SETUP_CLEANUP("Run Find And Results", TestDal, dal_run_find_results,
                           setup_dal_db, cleanup_dal_db) {
    FLContext fctx;
    faultline_initialize(&fctx, &dal_suite_alpha_beta, t->arena);
    fctx.run_start_time = DAL_TEST_START_TIME;

    dal_test_add_summary(&fctx, 0, FL_PASS);
    FLTestSummary *fail
        = dal_test_add_failure(&fctx, 1, FL_FAIL, FL_TEST_FAILURE, FL_DISCOVERY_PHASE);
    faultline_test_summary_add_fault(fail, 0, FL_FAIL, FAULT_NO_RESOURCE, "boom", "",
                                     "alloc.c", 10);
    faultline_test_summary_add_fault(fail, 1, FL_FAIL, FAULT_NO_RESOURCE, "boom", "",
                                     "free.c", 20);

    t->db            = faultline_init_database(t->path);
    i64 const run_id = faultline_run_record(t->db, &fctx);

    DalRunCollector found = {0};
    FL_ASSERT_TRUE(faultline_run_find(t->db, run_id, dal_collect_run, &found));
    FL_ASSERT_EQ(1, found.count);
    FL_ASSERT_EQ((int)run_id, (int)found.ids[0]);

    DalRunCollector missing = {0};
    FL_ASSERT_FALSE(faultline_run_find(t->db, run_id + 1, dal_collect_run, &missing));
    FL_ASSERT_EQ(0, missing.count);

    DalResultCollector results = {0};
    faultline_result_list(t->db, run_id, dal_collect_result, &results);
    FL_ASSERT_EQ(2, results.count);
    FL_ASSERT_STR_EQ("alpha", results.names[0]);
    FL_ASSERT_FALSE(results.has_file[0]);
    FL_ASSERT_STR_EQ("beta", results.names[1]);
    FL_ASSERT_EQ(FL_FAIL, results.codes[1]);
    FL_ASSERT_STR_EQ("alloc.c", results.files[1]);
    FL_ASSERT_EQ(10, results.lines[1]);
}

// Two runs of a suite that start in the same second: the latest is the one with the
// larger id. It passed, so the latest failures are empty while all failures hold the
// earlier run's fault.
FL_TYPE_TEST_SETUP_CLEANUP("Latest Failures By Run Id", TestDal,
                           dal_failures_latest_by_id, setup_dal_db, cleanup_dal_db) {
    FLContext fctx;
    faultline_initialize(&fctx, &dal_suite_alpha, t->arena);
    fctx.run_start_time = DAL_TEST_START_TIME;

    FLTestSummary *fail = dal_test_add_failure(&fctx, 0, FL_INVALID_FREE,
                                               FL_INVALID_FREE_FAILURE, FL_INJECTION_PHASE);
    faultline_test_summary_add_fault(fail, 1, FL_INVALID_FREE,
                                     (void const *)(uintptr_t)DAL_TEST_RESOURCE,
                                     "invalid free", "", "free.c", 20);

    t->db                = faultline_init_database(t->path);
    i64 const failed_run = faultline_run_record(t->db, &fctx);

    DalTestOutcome const pass[] = {{FL_PASS, 1, 0.1}};
    dal_test_record(t, &dal_suite_alpha, DAL_TEST_START_TIME, pass);

    DalFailureCollector latest = {0};
    faultline_failure_list(t->db, NULL, 0, FL_FAILURES_LATEST, dal_collect_failure,
                           &latest);
    FL_ASSERT_EQ(0, latest.count);

    DalFailureCollector all = {0};
    faultline_failure_list(t->db, NULL, 0, FL_FAILURES_ALL, dal_collect_failure, &all);
    FL_ASSERT_EQ(1, all.count);
    FL_ASSERT_EQ((int)failed_run, (int)all.run_ids[0]);
    FL_ASSERT_STR_EQ("free.c", all.files[0]);
    FL_ASSERT_EQ(DAL_TEST_RESOURCE, (int)all.resources[0]);
}

// Suites come most recently run first, with totals over their runs. Finding an unknown
// suite visits nothing.
FL_TYPE_TEST_SETUP_CLEANUP("Suites By Latest Run", TestDal, dal_suite_list_find,
                           setup_dal_db, cleanup_dal_db) {
    DalTestOutcome const one_pass[]  = {{FL_PASS, 2, 0.5}};
    DalTestOutcome const pass_fail[] = {{FL_PASS, 1, 1.0}, {FL_FAIL, 3, 1.0}};

    t->db = faultline_init_database(t->path);
    dal_test_record(t, &dal_suite_other, DAL_TEST_JAN1_0900, one_pass);
    dal_test_record(t, &dal_suite_alpha, DAL_TEST_JAN2_1000, one_pass);
    dal_test_record(t, &dal_suite_alpha_beta, DAL_TEST_JAN3_1000, pass_fail);

    DalSuiteCollector suites = {0};
    faultline_suite_list(t->db, 0, dal_collect_suite, &suites);
    FL_ASSERT_EQ(2, suites.count);
    FL_ASSERT_STR_EQ("DalSuite", suites.names[0]);
    FL_ASSERT_EQ(2, (int)suites.runs[0]);
    FL_ASSERT_EQ(2, (int)suites.cases[0]);
    FL_ASSERT_STR_EQ("OtherSuite", suites.names[1]);

    // DalSuite's pass rates are 1.0 and 0.5, its fault sites 2 and 1 + 3, and one of its
    // results failed.
    DalSuiteCollector found = {0};
    FL_ASSERT_TRUE(faultline_suite_find(t->db, "DalSuite", dal_collect_suite, &found));
    FL_ASSERT_STR_EQ("2024-01-02T10:00:00Z", found.first_runs[0]);
    FL_ASSERT_STR_EQ("2024-01-03T10:00:00Z", found.last_runs[0]);
    FL_ASSERT_EQ(750, (int)(found.pass_rates[0] * 1000.0 + 0.5));
    FL_ASSERT_EQ(6, (int)found.fault_sites[0]);
    FL_ASSERT_EQ(1, (int)found.failures[0]);

    DalSuiteCollector missing = {0};
    FL_ASSERT_FALSE(
        faultline_suite_find(t->db, "NoSuchSuite", dal_collect_suite, &missing));
    FL_ASSERT_EQ(0, missing.count);
}

// A case's baseline stays at its first run while its latest values follow the most
// recent run, across three runs and a failure.
FL_TYPE_TEST_SETUP_CLEANUP("Case History", TestDal, dal_case_history, setup_dal_db,
                           cleanup_dal_db) {
    DalTestOutcome const first[]  = {{FL_PASS, 10, 1.0}};
    DalTestOutcome const second[] = {{FL_PASS, 7, 1.5}};
    DalTestOutcome const third[]  = {{FL_FAIL, 5, 2.0}};

    t->db = faultline_init_database(t->path);
    dal_test_record(t, &dal_suite_alpha, DAL_TEST_JAN1_0900, first);
    dal_test_record(t, &dal_suite_alpha, DAL_TEST_JAN2_1000, second);
    dal_test_record(t, &dal_suite_alpha, DAL_TEST_JAN3_1000, third);

    DalCaseCollector cases = {0};
    faultline_case_list(t->db, NULL, 0, dal_collect_case, &cases);
    FL_ASSERT_EQ(1, cases.count);
    FL_ASSERT_EQ(3, (int)cases.appearances[0]);
    FL_ASSERT_EQ(1, (int)cases.failures[0]);
    FL_ASSERT_TRUE(cases.has_baseline[0]);
    FL_ASSERT_EQ(10, (int)cases.baseline_sites[0]);
    FL_ASSERT_EQ(5, (int)cases.last_sites[0]);
}

// Every case starts at 10 fault sites and 1 second. The next run drops coverage, doubles
// runtime, does both, changes nothing, and raises coverage. Only the first three
// regressed, and each row says which way.
FL_TYPE_TEST_SETUP_CLEANUP("Regressions Classified", TestDal, dal_regressions_classified,
                           setup_dal_db, cleanup_dal_db) {
    DalTestOutcome const baseline[] = {{FL_PASS, 10, 1.0},
                                       {FL_PASS, 10, 1.0},
                                       {FL_PASS, 10, 1.0},
                                       {FL_PASS, 10, 1.0},
                                       {FL_PASS, 10, 1.0}};
    DalTestOutcome const next[]     = {{FL_PASS, 7, 1.0},
                                       {FL_PASS, 10, 2.0},
                                       {FL_PASS, 7, 2.0},
                                       {FL_PASS, 10, 1.0},
                                       {FL_PASS, 12, 1.0}};

    t->db = faultline_init_database(t->path);
    dal_test_record(t, &dal_suite_classes, DAL_TEST_JAN1_0900, baseline);
    dal_test_record(t, &dal_suite_classes, DAL_TEST_JAN2_1000, next);

    DalRegressionCollector c = {0};
    faultline_regression_list(t->db, "ClassSuite", 0, dal_test_threshold_pct,
                              dal_collect_regression, &c);
    FL_ASSERT_EQ(3, c.count);

    int const cov  = dal_regression_index(&c, "cov_only");
    int const rt   = dal_regression_index(&c, "rt_only");
    int const both = dal_regression_index(&c, "both");
    FL_ASSERT_TRUE(cov >= 0 && rt >= 0 && both >= 0);
    FL_ASSERT_TRUE(dal_regression_index(&c, "clean") < 0);
    FL_ASSERT_TRUE(dal_regression_index(&c, "cov_up") < 0);

    FL_ASSERT_TRUE(c.coverage[cov] && !c.runtime[cov]);
    FL_ASSERT_TRUE(!c.coverage[rt] && c.runtime[rt]);
    FL_ASSERT_TRUE(c.coverage[both] && c.runtime[both]);
    FL_ASSERT_EQ(10, (int)c.baseline_sites[cov]);
    FL_ASSERT_EQ(7, (int)c.last_sites[cov]);

    DalRegressionCollector empty = {0};
    faultline_regression_list(t->db, "NoSuchSuite", 0, dal_test_threshold_pct,
                              dal_collect_regression, &empty);
    FL_ASSERT_EQ(0, empty.count);
}

// Resetting a baseline to the latest run clears the case's regression. Filters that match
// no case change nothing.
FL_TYPE_TEST_SETUP_CLEANUP("Baseline Reset", TestDal, dal_baseline_reset, setup_dal_db,
                           cleanup_dal_db) {
    DalTestOutcome const ten[]   = {{FL_PASS, 10, 1.0}};
    DalTestOutcome const seven[] = {{FL_PASS, 7, 1.0}};

    t->db = faultline_init_database(t->path);
    dal_test_record(t, &dal_suite_alpha, DAL_TEST_JAN1_0900, ten);
    dal_test_record(t, &dal_suite_alpha, DAL_TEST_JAN2_1000, seven);

    DalRegressionCollector before = {0};
    faultline_regression_list(t->db, "DalSuite", 0, dal_test_threshold_pct,
                              dal_collect_regression, &before);
    FL_ASSERT_EQ(1, before.count);

    FL_ASSERT_EQ(0, (int)faultline_baseline_reset(t->db, "OtherSuite", NULL));
    FL_ASSERT_EQ(0, (int)faultline_baseline_reset(t->db, NULL, "no_such_case"));
    FL_ASSERT_EQ(1, (int)faultline_baseline_reset(t->db, "DalSuite", "alpha"));

    DalRegressionCollector after = {0};
    faultline_regression_list(t->db, "DalSuite", 0, dal_test_threshold_pct,
                              dal_collect_regression, &after);
    FL_ASSERT_EQ(0, after.count);

    DalCaseCollector cases = {0};
    faultline_case_list(t->db, "DalSuite", 0, dal_collect_case, &cases);
    FL_ASSERT_EQ(1, cases.count);
    FL_ASSERT_EQ(7, (int)cases.baseline_sites[0]);
}

// One suite over three days. January 1 has two runs, so its pass rate is the mean of 0.8
// and 1.0 and its failures are 1 + 0. Each later day carries the day before it.
FL_TYPE_TEST_SETUP_CLEANUP("Trends By Day", TestDal, dal_trends_by_day, setup_dal_db,
                           cleanup_dal_db) {
    DalTestOutcome const four_of_five[] = {{FL_PASS, 1, 1.0},
                                           {FL_PASS, 1, 1.0},
                                           {FL_PASS, 1, 1.0},
                                           {FL_PASS, 1, 1.0},
                                           {FL_FAIL, 1, 1.0}};
    DalTestOutcome const five_of_five[] = {{FL_PASS, 1, 1.0},
                                           {FL_PASS, 1, 1.0},
                                           {FL_PASS, 1, 1.0},
                                           {FL_PASS, 1, 1.0},
                                           {FL_PASS, 1, 1.0}};
    DalTestOutcome const two_of_five[]  = {{FL_PASS, 1, 1.0},
                                           {FL_PASS, 1, 1.0},
                                           {FL_FAIL, 1, 1.0},
                                           {FL_FAIL, 1, 1.0},
                                           {FL_FAIL, 1, 1.0}};

    t->db = faultline_init_database(t->path);
    dal_test_record(t, &dal_suite_outcomes, DAL_TEST_JAN1_0900, four_of_five);
    dal_test_record(t, &dal_suite_outcomes, DAL_TEST_JAN1_1500, five_of_five);
    dal_test_record(t, &dal_suite_outcomes, DAL_TEST_JAN2_1000, two_of_five);
    dal_test_record(t, &dal_suite_outcomes, DAL_TEST_JAN3_1000, five_of_five);

    DalTrendCollector c = {0};
    faultline_trend_list(t->db, "OutcomeSuite", 0, dal_collect_trend, &c);
    FL_ASSERT_EQ(3, c.count);

    // Newest day first.
    FL_ASSERT_STR_EQ("2024-01-03", c.days[0]);
    FL_ASSERT_STR_EQ("2024-01-02", c.days[1]);
    FL_ASSERT_STR_EQ("2024-01-01", c.days[2]);

    FL_ASSERT_EQ(2, c.runs[2]);
    FL_ASSERT_EQ(900, c.pass_milli[2]);
    FL_ASSERT_EQ(1, c.failures[2]);
    FL_ASSERT_FALSE(c.has_previous[2]);

    FL_ASSERT_TRUE(c.has_previous[1]);
    FL_ASSERT_EQ(400, c.pass_milli[1]);
    FL_ASSERT_EQ(900, c.previous_pass_milli[1]);
    FL_ASSERT_EQ(3, c.failures[1]);
    FL_ASSERT_EQ(1, c.previous_failures[1]);

    FL_ASSERT_TRUE(c.has_previous[0]);
    FL_ASSERT_EQ(1000, c.pass_milli[0]);
    FL_ASSERT_EQ(400, c.previous_pass_milli[0]);

    DalTrendCollector empty = {0};
    faultline_trend_list(t->db, "NoSuchSuite", 0, dal_collect_trend, &empty);
    FL_ASSERT_EQ(0, empty.count);
}

// alloc.c:10 records three faults across both cases and free.c:20 records one, so
// alloc.c:10 comes first.
FL_TYPE_TEST_SETUP_CLEANUP("Hotspots Ranked", TestDal, dal_hotspots_ranked, setup_dal_db,
                           cleanup_dal_db) {
    FLContext fctx;
    faultline_initialize(&fctx, &dal_suite_alpha_beta, t->arena);

    FLTestSummary *alpha
        = dal_test_add_failure(&fctx, 0, FL_LEAK, FL_LEAK_FAILURE, FL_INJECTION_PHASE);
    faultline_test_summary_add_fault(alpha, 1, FL_LEAK, FAULT_NO_RESOURCE, "leak", "",
                                     "alloc.c", 10);
    faultline_test_summary_add_fault(alpha, 2, FL_LEAK, FAULT_NO_RESOURCE, "leak", "",
                                     "alloc.c", 10);
    faultline_test_summary_add_fault(alpha, 2, FL_LEAK, FAULT_NO_RESOURCE, "leak", "",
                                     "free.c", 20);

    FLTestSummary *beta
        = dal_test_add_failure(&fctx, 1, FL_LEAK, FL_LEAK_FAILURE, FL_INJECTION_PHASE);
    faultline_test_summary_add_fault(beta, 1, FL_LEAK, FAULT_NO_RESOURCE, "leak", "",
                                     "alloc.c", 10);

    t->db = faultline_init_database(t->path);
    faultline_run_record(t->db, &fctx);

    DalHotspotCollector c = {0};
    faultline_hotspot_list(t->db, "DalSuite", 0, dal_collect_hotspot, &c);
    FL_ASSERT_EQ(2, c.count);

    FL_ASSERT_STR_EQ("alloc.c", c.files[0]);
    FL_ASSERT_EQ(10, c.lines[0]);
    FL_ASSERT_EQ(3, (int)c.faults[0]);
    FL_ASSERT_EQ(2, (int)c.cases[0]);

    FL_ASSERT_STR_EQ("free.c", c.files[1]);
    FL_ASSERT_EQ(20, c.lines[1]);
    FL_ASSERT_EQ(1, (int)c.faults[1]);
    FL_ASSERT_EQ(1, (int)c.cases[1]);

    DalHotspotCollector empty = {0};
    faultline_hotspot_list(t->db, "NoSuchSuite", 0, dal_collect_hotspot, &empty);
    FL_ASSERT_EQ(0, empty.count);
}
