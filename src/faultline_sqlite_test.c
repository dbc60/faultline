/**
 * @file faultline_sqlite_test.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-09-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline_sqlite.h>

#include "faultline_sqlite_test.h" // TestSchema
#include <faultline/fl_test.h>
#include <faultline/fl_try.h>
#include <faultline/fl_exception_assert.h> // assert macros and fl_unexpected_failure declaration
#include <faultline/arena.h>               // new_arena, release_arena
#include <faultline/fl_test_summary.h> // FLTestSummary, init_faultline_test_summary
#include <faultline/fl_result_codes.h> // FL_PASS
#include <sqlite/sqlite3.h>

#include <stdio.h>
#include <string.h> // strcmp
#include <time.h>   // time_t

#ifdef _WIN32
#include <io.h>       // _chmod
#include <sys/stat.h> // _S_IREAD, _S_IWRITE
#define FL_RO_MODE _S_IREAD
#define FL_RW_MODE (_S_IREAD | _S_IWRITE)
#define fl_chmod   _chmod
#else
#include <sys/stat.h> // chmod, S_IRUSR, S_IWUSR
#define FL_RO_MODE S_IRUSR
#define FL_RW_MODE (S_IRUSR | S_IWUSR)
#define fl_chmod   chmod
#endif

///////////////////////////
//  Test Infrastructure  //
///////////////////////////

// Helper function to verify table exists
static void verify_table_exists(sqlite3 *db, char const *table_name) {
    sqlite3_stmt *stmt;
    char          query[256];

    snprintf(query, sizeof(query),
             "SELECT name FROM sqlite_master WHERE type='table' AND name='%s'",
             table_name);

    FL_ASSERT_EQ_INT(SQLITE_OK, sqlite3_prepare_v2(db, query, -1, &stmt, NULL));
    FL_ASSERT_EQ_INT(SQLITE_ROW, sqlite3_step(stmt)); // Should find the table
    sqlite3_finalize(stmt);
}

// Helper function to verify view exists
static void verify_view_exists(sqlite3 *db, char const *view_name) {
    sqlite3_stmt *stmt;
    char          query[256];

    snprintf(query, sizeof(query),
             "SELECT name FROM sqlite_master WHERE type='view' AND name='%s'",
             view_name);

    FL_ASSERT_EQ_INT(SQLITE_OK, sqlite3_prepare_v2(db, query, -1, &stmt, NULL));
    FL_ASSERT_EQ_INT(SQLITE_ROW, sqlite3_step(stmt)); // Should find the view
    sqlite3_finalize(stmt);
}

// Helper function to clean up test database
static void cleanup_test_db(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    remove(t->test_db);
}

////////////////////////////////
//  Core Functionality Tests  //
////////////////////////////////

//  1. Happy Path - Schema Creation
static void setup_schema_creation_db(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_schema_creation.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Schema Creation", TestSchema, schema_creates_database,
                           setup_schema_creation_db, cleanup_test_db) {
    // Should create database and schema without throwing
    faultline_sqlite_init_schema(t->test_db);

    // Verify database file was created
    FILE *f;
    fopen_s(&f, t->test_db, "r");
    FL_ASSERT_NOT_NULL(f);
    fclose(f);
}

//  2. Core Tables Verification
static void setup_core_tables(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_core_tables.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Verify Tables", TestSchema, schema_creates_core_tables,
                           setup_core_tables, cleanup_test_db) {
    sqlite3 *db;

    faultline_sqlite_init_schema(t->test_db);

    // Open database and verify core tables exist
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));

    verify_table_exists(db, "test_suites");
    verify_table_exists(db, "raw_test_runs");
    verify_table_exists(db, "raw_test_summaries");
    verify_table_exists(db, "raw_faults");
    verify_table_exists(db, "schema_info");

    sqlite3_close_v2(db);
}

//  3. Schema Version Tracking
static void setup_version_tracking(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_version.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Version Tracking", TestSchema, schema_sets_version,
                           setup_version_tracking, cleanup_test_db) {
    sqlite3      *db;
    sqlite3_stmt *stmt;
    int           version;

    faultline_sqlite_init_schema(t->test_db);

    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));

    // Verify schema version was recorded
    FL_ASSERT_EQ_INT(SQLITE_OK, sqlite3_prepare_v2(db, "SELECT version FROM schema_info",
                                                   -1, &stmt, NULL));
    FL_ASSERT_EQ_INT(SQLITE_ROW, sqlite3_step(stmt));

    version = sqlite3_column_int(stmt, 0);
    FL_ASSERT_EQ_INT(FL_SCHEMA_VERSION, version);

    sqlite3_finalize(stmt);
    sqlite3_close_v2(db);
}

////////////////////////////////////
//  Foreign Key Constraint Tests  //
////////////////////////////////////

//  4. Foreign Key Relationships
static void setup_foreign_keys(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_foreign_keys.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Foreign Keys", TestSchema, schema_creates_foreign_keys,
                           setup_foreign_keys, cleanup_test_db) {
    sqlite3 *db;

    faultline_sqlite_init_schema(t->test_db);

    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READWRITE, NULL));

    // Enable foreign key checking
    sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);

    // Test foreign key constraint - should fail to insert run with invalid suite_id
    int rc = sqlite3_exec(
        db,
        "INSERT INTO raw_test_runs (suite_id, test_cases, tests_run, tests_passed, "
        "tests_passed_with_leaks, tests_failed, setups_failed, cleanups_failed, "
        "total_fault_sites, total_elapsed_time) "
        "VALUES (999, 1, 1, 1, 0, 0, 0, 0, 5, 1.0)",
        NULL, NULL, NULL);

    FL_ASSERT_NEQ_INT(SQLITE_OK, rc); // Should fail due to FK constraint

    sqlite3_close_v2(db);
}

///////////////////////////////////////
//  Analysis Tables and Views Tests  //
///////////////////////////////////////

//  5. Analysis Tables Creation
static void setup_analysis_tables(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_analysis_tables.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Analysis Tables", TestSchema, schema_creates_analysis_tables,
                           setup_analysis_tables, cleanup_test_db) {
    sqlite3 *db;

    faultline_sqlite_init_schema(t->test_db);

    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));
    verify_table_exists(db, "test_case_evolution");

    sqlite3_close_v2(db);
}

//  6. Views Creation
static void setup_views(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_views.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Create Views", TestSchema, schema_creates_views, setup_views,
                           cleanup_test_db) {
    sqlite3 *db;

    faultline_sqlite_init_schema(t->test_db);

    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));

    verify_view_exists(db, "latest_runs");

    sqlite3_close_v2(db);
}

//  6b. latest_runs counts passing cases (result_code = FL_PASS = 1), not 0
static void setup_latest_runs_counts(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_latest_runs_counts.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Latest Runs Counts", TestSchema, schema_counts_successes,
                           setup_latest_runs_counts, cleanup_test_db) {
    faultline_sqlite_init_schema(t->test_db);

    sqlite3 *db;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READWRITE, NULL));

    // One run with two cases: one pass (FL_PASS = 1) and one failure (FL_FAIL = 5).
    FL_ASSERT_EQ_INT(
        SQLITE_OK,
        sqlite3_exec(
            db,
            "INSERT INTO test_suites (suite_name) VALUES ('S');"
            "INSERT INTO raw_test_runs (suite_id, timestamp, test_cases, tests_run,"
            "  tests_passed, tests_passed_with_leaks, tests_failed, setups_failed,"
            "  cleanups_failed, total_fault_sites, total_elapsed_time) VALUES"
            "  (1,'2024-05-01 09:00:00',2,2,1,0,1,0,0,7,1.0);"
            "INSERT INTO raw_test_summaries (run_id, test_index, test_name, result_code,"
            "  elapsed_seconds, faults_exercised) VALUES"
            "  (1,0,'pass_case',1,0.5,3),"
            "  (1,1,'fail_case',5,0.5,4);",
            NULL, NULL, NULL));

    sqlite3_stmt *stmt;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_prepare_v2(db,
                                        "SELECT executed_test_cases, successful_cases "
                                        "FROM latest_runs WHERE suite_name = 'S'",
                                        -1, &stmt, NULL));
    FL_ASSERT_EQ_INT(SQLITE_ROW, sqlite3_step(stmt));
    FL_ASSERT_EQ_INT(2, sqlite3_column_int(stmt, 0)); // both cases executed
    FL_ASSERT_EQ_INT(1, sqlite3_column_int(stmt, 1)); // only the FL_PASS case counts
    sqlite3_finalize(stmt);

    sqlite3_close_v2(db);
}

/////////////////////////////////
//  Regression Tracking Tests  //
/////////////////////////////////

// Record one summary for `test_name` into an existing run, with the given
// result code, fault-site count, and elapsed time.
static void record_summary(sqlite3 *db, Arena *arena, int run_id, char const *test_name,
                           FLResultCode code, int fault_sites, double elapsed) {
    FLTestSummary summary;
    init_faultline_test_summary(&summary, arena, 0, code, "", NULL);
    summary.faults_exercised = fault_sites;
    summary.elapsed_seconds  = elapsed;
    faultline_record_test_summary(db, run_id, &summary, test_name);
}

// Run a single-value integer query, returning -1 if it produced no row.
static int query_int(sqlite3 *db, char const *sql) {
    sqlite3_stmt *stmt;
    int           result = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

// Collects FLRegression rows from faultline_for_each_regression so a test can
// assert on them after iteration (string fields are copied, since the callback's
// pointers are only valid during the call).
typedef struct {
    int       count;
    bool      coverage[8];
    bool      runtime[8];
    long long base_fs[8];
    long long last_fs[8];
    char      test_name[8][64];
} RegressionCollector;

static void collect_regression(FLRegression const *r, void *vctx) {
    RegressionCollector *c = (RegressionCollector *)vctx;
    int                  i = c->count;
    if (i < 8) {
        c->coverage[i] = r->coverage_regression;
        c->runtime[i]  = r->runtime_regression;
        c->base_fs[i]  = r->baseline_fault_sites;
        c->last_fs[i]  = r->last_fault_sites;
        snprintf(c->test_name[i], sizeof c->test_name[i], "%s", r->test_name);
    }
    c->count++;
}

static int collected_index(RegressionCollector const *c, char const *name) {
    for (int i = 0; i < c->count && i < 8; i++) {
        if (strcmp(c->test_name[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

//  7. Baseline freezes on first sighting; latest tracks the most recent run
static void setup_regression_tracking(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_regression_tracking.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Regression Baseline", TestSchema, schema_tracks_baseline,
                           setup_regression_tracking, cleanup_test_db) {
    faultline_sqlite_init_schema(t->test_db);

    sqlite3 *db;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READWRITE, NULL));

    Arena *arena = new_arena(0, 0);

    // First run establishes the baseline (10 fault sites). Later runs must not
    // disturb the baseline, even across three runs and a failure.
    int run1 = faultline_record_test_run_start(db, "RegrSuite", 1000);
    record_summary(db, arena, run1, "regr_test", FL_PASS, 10, 1.0);
    int run2 = faultline_record_test_run_start(db, "RegrSuite", 2000);
    record_summary(db, arena, run2, "regr_test", FL_PASS, 7, 1.5);
    int run3 = faultline_record_test_run_start(db, "RegrSuite", 3000);
    record_summary(db, arena, run3, "regr_test", FL_FAIL, 5, 2.0);

    sqlite3_stmt *stmt;
    FL_ASSERT_EQ_INT(
        SQLITE_OK,
        sqlite3_prepare_v2(db,
                           "SELECT baseline_fault_sites, last_fault_sites, "
                           "       baseline_run_id, last_run_id, total_appearances, "
                           "       total_failures "
                           "FROM test_case_evolution "
                           "WHERE suite_name = 'RegrSuite' AND test_name = 'regr_test'",
                           -1, &stmt, NULL));
    FL_ASSERT_EQ_INT(SQLITE_ROW, sqlite3_step(stmt));

    FL_ASSERT_EQ_INT(10, sqlite3_column_int(stmt, 0));   // baseline still frozen at run1
    FL_ASSERT_EQ_INT(5, sqlite3_column_int(stmt, 1));    // latest reflects run3
    FL_ASSERT_EQ_INT(run1, sqlite3_column_int(stmt, 2)); // baseline_run_id == run1
    FL_ASSERT_EQ_INT(run3, sqlite3_column_int(stmt, 3)); // last_run_id == run3
    FL_ASSERT_EQ_INT(3, sqlite3_column_int(stmt, 4));    // appeared in three runs
    FL_ASSERT_EQ_INT(1, sqlite3_column_int(stmt, 5));    // one failing run (run3)

    sqlite3_finalize(stmt);

    release_arena(&arena);
    sqlite3_close_v2(db);
}

//  8. The report classifies coverage/runtime regressions and excludes clean tests
static void setup_regression_report(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_regression_report.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Regression Report", TestSchema, schema_reports_regressions,
                           setup_regression_report, cleanup_test_db) {
    faultline_sqlite_init_schema(t->test_db);

    sqlite3 *db;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READWRITE, NULL));

    Arena *arena = new_arena(0, 0);

    // Baseline run: every test starts at 10 fault sites, 1.0s.
    int base = faultline_record_test_run_start(db, "ClassSuite", 1000);
    record_summary(db, arena, base, "cov_only", FL_PASS, 10, 1.0);
    record_summary(db, arena, base, "rt_only", FL_PASS, 10, 1.0);
    record_summary(db, arena, base, "both", FL_PASS, 10, 1.0);
    record_summary(db, arena, base, "clean", FL_PASS, 10, 1.0);
    record_summary(db, arena, base, "cov_up", FL_PASS, 10, 1.0);

    // Second run varies each test: coverage drop, runtime +100%, both, neither,
    // and a coverage *increase* (which must not be flagged).
    int next = faultline_record_test_run_start(db, "ClassSuite", 2000);
    record_summary(db, arena, next, "cov_only", FL_PASS, 7, 1.0);
    record_summary(db, arena, next, "rt_only", FL_PASS, 10, 2.0);
    record_summary(db, arena, next, "both", FL_PASS, 7, 2.0);
    record_summary(db, arena, next, "clean", FL_PASS, 10, 1.0);
    record_summary(db, arena, next, "cov_up", FL_PASS, 12, 1.0);

    RegressionCollector c = {0};
    int n = faultline_for_each_regression(db, "ClassSuite", 0, 20.0, collect_regression,
                                          &c);

    // Only cov_only, rt_only, and both regressed; clean and cov_up are excluded.
    FL_ASSERT_EQ_INT(3, n);
    FL_ASSERT_EQ_INT(3, c.count);

    int i_cov  = collected_index(&c, "cov_only");
    int i_rt   = collected_index(&c, "rt_only");
    int i_both = collected_index(&c, "both");
    FL_ASSERT_TRUE(i_cov >= 0 && i_rt >= 0 && i_both >= 0);
    FL_ASSERT_TRUE(collected_index(&c, "clean") < 0);
    FL_ASSERT_TRUE(collected_index(&c, "cov_up") < 0);

    FL_ASSERT_TRUE(c.coverage[i_cov] && !c.runtime[i_cov]);
    FL_ASSERT_TRUE(!c.coverage[i_rt] && c.runtime[i_rt]);
    FL_ASSERT_TRUE(c.coverage[i_both] && c.runtime[i_both]);
    FL_ASSERT_EQ_INT(7, (int)c.last_fs[i_cov]);
    FL_ASSERT_EQ_INT(10, (int)c.base_fs[i_cov]);

    // The suite filter scopes the query: a non-existent suite yields nothing.
    RegressionCollector empty = {0};
    FL_ASSERT_EQ_INT(0, faultline_for_each_regression(db, "NoSuchSuite", 0, 20.0,
                                                      collect_regression, &empty));

    release_arena(&arena);
    sqlite3_close_v2(db);
}

//  8b. Re-pinning the baseline clears regressions for the chosen scope
static void setup_baseline_reset(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_baseline_reset.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Baseline Reset", TestSchema, schema_resets_baseline,
                           setup_baseline_reset, cleanup_test_db) {
    faultline_sqlite_init_schema(t->test_db);

    sqlite3 *db;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READWRITE, NULL));

    Arena *arena = new_arena(0, 0);

    // Baseline at 10 fault sites, then a run that drops to 7 -> coverage regression.
    int run1 = faultline_record_test_run_start(db, "ResetSuite", 1000);
    record_summary(db, arena, run1, "rt", FL_PASS, 10, 1.0);
    int run2 = faultline_record_test_run_start(db, "ResetSuite", 2000);
    record_summary(db, arena, run2, "rt", FL_PASS, 7, 1.0);

    RegressionCollector before = {0};
    FL_ASSERT_EQ_INT(1, faultline_for_each_regression(db, "ResetSuite", 0, 20.0,
                                                      collect_regression, &before));

    // A non-matching filter re-pins nothing and leaves the regression in place.
    FL_ASSERT_EQ_INT(0, faultline_reset_baselines(db, "OtherSuite", NULL));
    RegressionCollector still = {0};
    FL_ASSERT_EQ_INT(1, faultline_for_each_regression(db, "ResetSuite", 0, 20.0,
                                                      collect_regression, &still));

    // Re-pinning the baseline to the latest run clears the regression.
    FL_ASSERT_EQ_INT(1, faultline_reset_baselines(db, "ResetSuite", NULL));
    RegressionCollector after = {0};
    FL_ASSERT_EQ_INT(0, faultline_for_each_regression(db, "ResetSuite", 0, 20.0,
                                                      collect_regression, &after));

    // The stored baseline now matches the latest observation.
    FL_ASSERT_EQ_INT(7, query_int(db, "SELECT baseline_fault_sites FROM "
                                      "test_case_evolution WHERE test_name = 'rt'"));

    release_arena(&arena);
    sqlite3_close_v2(db);
}

//  9. Opening a pre-v2 database rebuilds test_case_evolution (migration)
static void setup_schema_migration(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_schema_migration.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Schema Migration v1 to v2", TestSchema, schema_migrates_v1,
                           setup_schema_migration, cleanup_test_db) {
    // Seed a v1 database: old test_case_evolution shape, the retired trigger, a
    // garbage row, and schema_info version 1.
    sqlite3 *seed;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &seed,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));
    FL_ASSERT_EQ_INT(
        SQLITE_OK,
        sqlite3_exec(
            seed,
            "CREATE TABLE schema_info (version INTEGER PRIMARY KEY, applied_date TEXT);"
            "CREATE TABLE test_case_evolution (id INTEGER PRIMARY KEY, suite_name TEXT,"
            "    test_name TEXT, avg_fault_sites REAL);"
            "CREATE TRIGGER update_test_evolution AFTER INSERT ON test_case_evolution "
            "    BEGIN SELECT 1; END;"
            "INSERT INTO test_case_evolution (suite_name, test_name) VALUES ('x','y');"
            "INSERT INTO schema_info (version, applied_date) VALUES (1, 'seed');",
            NULL, NULL, NULL));
    sqlite3_close_v2(seed);

    // Re-applying the schema must upgrade the database in place.
    faultline_sqlite_init_schema(t->test_db);

    sqlite3 *db;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));

    FL_ASSERT_EQ_INT(FL_SCHEMA_VERSION, query_int(db, "SELECT MAX(version) FROM "
                                                      "schema_info"));
    FL_ASSERT_EQ_INT(1, query_int(db, "SELECT COUNT(*) FROM "
                                      "pragma_table_info('test_case_evolution') "
                                      "WHERE name='baseline_fault_sites'"));
    FL_ASSERT_EQ_INT(0,
                     query_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE "
                                   "type='trigger' AND name='update_test_evolution'"));
    FL_ASSERT_EQ_INT(0, query_int(db, "SELECT COUNT(*) FROM test_case_evolution"));

    sqlite3_close_v2(db);
}

////////////////////////////
//  Trend Tracking Tests  //
////////////////////////////

// Collects FLSuiteTrend rows from faultline_for_each_trend. Pass rates are kept
// as integer per-mille so the test can assert them without float comparison.
typedef struct {
    int  count;
    char day[8][16];
    int  total_runs[8];
    int  pass_milli[8];
    int  fails[8];
    bool has_prev[8];
    int  prev_pass_milli[8];
    int  prev_fails[8];
} TrendCollector;

static void collect_trend(FLSuiteTrend const *t, void *vctx) {
    TrendCollector *c = (TrendCollector *)vctx;
    int             i = c->count;
    if (i < 8) {
        snprintf(c->day[i], sizeof c->day[i], "%s", t->day);
        c->total_runs[i]      = t->total_runs;
        c->pass_milli[i]      = (int)(t->avg_pass_rate * 1000.0 + 0.5);
        c->fails[i]           = (int)t->total_failures;
        c->has_prev[i]        = t->has_prev;
        c->prev_pass_milli[i] = (int)(t->prev_pass_rate * 1000.0 + 0.5);
        c->prev_fails[i]      = (int)t->prev_total_failures;
    }
    c->count++;
}

//  10. Trends aggregate runs per day and carry the prior day's values (LAG)
static void setup_trend_report(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_trend_report.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Suite Trends", TestSchema, schema_reports_trends,
                           setup_trend_report, cleanup_test_db) {
    faultline_sqlite_init_schema(t->test_db);

    sqlite3 *db;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READWRITE, NULL));

    // Seed one suite across three days; day 1 has two runs to exercise the daily
    // aggregation (avg pass 0.9, summed failures 1).
    FL_ASSERT_EQ_INT(
        SQLITE_OK,
        sqlite3_exec(
            db,
            "INSERT INTO test_suites (suite_name) VALUES ('TrendSuite');"
            "INSERT INTO raw_test_runs (suite_id, timestamp, test_cases, tests_run,"
            "  tests_passed, tests_passed_with_leaks, tests_failed, setups_failed,"
            "  cleanups_failed, total_fault_sites, total_elapsed_time, pass_rate) VALUES"
            "  (1,'2024-01-01 09:00:00',5,5,4,0,1,0,0,20,2.0,0.8),"
            "  (1,'2024-01-01 15:00:00',5,5,5,0,0,0,0,30,4.0,1.0),"
            "  (1,'2024-01-02 10:00:00',5,5,2,0,3,0,0,10,6.0,0.5),"
            "  (1,'2024-01-03 10:00:00',5,5,5,0,0,0,0,40,1.0,1.0);",
            NULL, NULL, NULL));

    TrendCollector c = {0};
    int            n = faultline_for_each_trend(db, "TrendSuite", 0, collect_trend, &c);

    FL_ASSERT_EQ_INT(3, n); // three distinct days
    FL_ASSERT_EQ_INT(3, c.count);

    // Rows are most-recent-day first: day3, day2, day1.
    FL_ASSERT_STR_EQ("2024-01-03", c.day[0]);
    FL_ASSERT_STR_EQ("2024-01-02", c.day[1]);
    FL_ASSERT_STR_EQ("2024-01-01", c.day[2]);

    // Day 1 aggregates its two runs and is the suite's first day (no previous).
    FL_ASSERT_EQ_INT(2, c.total_runs[2]);
    FL_ASSERT_EQ_INT(900, c.pass_milli[2]); // (0.8 + 1.0) / 2
    FL_ASSERT_EQ_INT(1, c.fails[2]);
    FL_ASSERT_FALSE(c.has_prev[2]);

    // Day 2's previous-day (LAG) values are day 1's aggregates.
    FL_ASSERT_TRUE(c.has_prev[1]);
    FL_ASSERT_EQ_INT(500, c.pass_milli[1]);
    FL_ASSERT_EQ_INT(900, c.prev_pass_milli[1]);
    FL_ASSERT_EQ_INT(3, c.fails[1]);
    FL_ASSERT_EQ_INT(1, c.prev_fails[1]);

    // Day 3's previous is day 2.
    FL_ASSERT_TRUE(c.has_prev[0]);
    FL_ASSERT_EQ_INT(1000, c.pass_milli[0]);
    FL_ASSERT_EQ_INT(500, c.prev_pass_milli[0]);

    // The suite filter scopes the query: a non-existent suite yields nothing.
    TrendCollector empty = {0};
    FL_ASSERT_EQ_INT(0, faultline_for_each_trend(db, "NoSuchSuite", 0, collect_trend,
                                                 &empty));

    sqlite3_close_v2(db);
}

/////////////////////////////
//  Fault Hotspot Tests     //
/////////////////////////////

typedef struct {
    int  count;
    char file[8][64];
    int  line[8];
    int  failures[8];
    int  tests[8];
} HotspotCollector;

static void collect_hotspot(FLFaultHotspot const *h, void *vctx) {
    HotspotCollector *c = (HotspotCollector *)vctx;
    int               i = c->count;
    if (i < 8) {
        snprintf(c->file[i], sizeof c->file[i], "%s", h->source_file);
        c->line[i]     = h->source_line;
        c->failures[i] = (int)h->failure_count;
        c->tests[i]    = (int)h->tests_affected;
    }
    c->count++;
}

//  11. Hotspots rank source locations by fault count, worst first
static void setup_hotspot_report(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_hotspot_report.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Fault Hotspots", TestSchema, schema_reports_hotspots,
                           setup_hotspot_report, cleanup_test_db) {
    faultline_sqlite_init_schema(t->test_db);

    sqlite3 *db;
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READWRITE, NULL));

    // One run, two test cases. alloc.c:10 faults three times across both cases;
    // free.c:20 faults once.
    FL_ASSERT_EQ_INT(
        SQLITE_OK,
        sqlite3_exec(
            db,
            "INSERT INTO test_suites (suite_name) VALUES ('H');"
            "INSERT INTO raw_test_runs (suite_id, timestamp, test_cases, tests_run,"
            "  tests_passed, tests_passed_with_leaks, tests_failed, setups_failed,"
            "  cleanups_failed, total_fault_sites, total_elapsed_time) VALUES"
            "  (1,'2024-06-01 09:00:00',2,2,0,0,2,0,0,4,1.0);"
            "INSERT INTO raw_test_summaries (run_id, test_index, test_name, result_code,"
            "  elapsed_seconds, faults_exercised) VALUES"
            "  (1,0,'case_a',5,0.5,2),(1,1,'case_b',5,0.5,2);"
            "INSERT INTO raw_faults (summary_id, fault_index, result_code,"
            "  exception_reason, details, source_file, source_line) VALUES"
            "  (1,0,5,'r','d','alloc.c',10),"
            "  (1,1,5,'r','d','alloc.c',10),"
            "  (2,0,5,'r','d','alloc.c',10),"
            "  (1,2,5,'r','d','free.c',20);",
            NULL, NULL, NULL));

    HotspotCollector c = {0};
    int              n = faultline_for_each_hotspot(db, "H", 0, collect_hotspot, &c);

    FL_ASSERT_EQ_INT(2, n);
    FL_ASSERT_EQ_INT(2, c.count);

    // Worst first: alloc.c:10 with 3 faults across 2 cases.
    FL_ASSERT_STR_EQ("alloc.c", c.file[0]);
    FL_ASSERT_EQ_INT(10, c.line[0]);
    FL_ASSERT_EQ_INT(3, c.failures[0]);
    FL_ASSERT_EQ_INT(2, c.tests[0]);

    // Then free.c:20 with a single fault in one case.
    FL_ASSERT_STR_EQ("free.c", c.file[1]);
    FL_ASSERT_EQ_INT(20, c.line[1]);
    FL_ASSERT_EQ_INT(1, c.failures[1]);
    FL_ASSERT_EQ_INT(1, c.tests[1]);

    // The suite filter scopes the query: a non-existent suite yields nothing.
    HotspotCollector empty = {0};
    FL_ASSERT_EQ_INT(0, faultline_for_each_hotspot(db, "NoSuchSuite", 0, collect_hotspot,
                                                   &empty));

    sqlite3_close_v2(db);
}

////////////////////////////
//  Error Handling Tests  //
////////////////////////////

//  7. Invalid Path Handling
static void setup_invalid_path(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "/invalid/nonexistent/path/test.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Invalid Path", TestSchema, schema_invalid_path_throws,
                           setup_invalid_path, cleanup_test_db) {
    bool throw_catch = false;

    // Test with invalid/inaccessible path
    FL_TRY {
        faultline_sqlite_init_schema(t->test_db);
    }
    FL_CATCH(faultline_db_create_failed) {
        throw_catch = true; // success
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(throw_catch);
}

//  8. Permission Denied Handling
//
// Create a regular file and mark it read-only, then point the schema
// initializer at it. init_schema opens with SQLITE_OPEN_READWRITE, so opening a
// read-only file fails (and even if that were tolerated, the first CREATE TABLE
// would), and it throws faultline_db_create_failed. The read-only attribute is
// honored even for an elevated process, so this is reliable on CI -- unlike
// relying on a system directory being write-protected.
static void setup_permission_denied(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_permission_denied.db";

    FILE *f = NULL;
    fopen_s(&f, t->test_db, "wb");
    if (f != NULL) {
        (void)fclose(f);
    }
    (void)fl_chmod(t->test_db, FL_RO_MODE);
}

// remove() cannot delete a read-only file on Windows, so restore write access
// before deleting.
static void cleanup_permission_denied(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    (void)fl_chmod(t->test_db, FL_RW_MODE);
    remove(t->test_db);
}

FL_TYPE_TEST_SETUP_CLEANUP("Permission Denied", TestSchema,
                           schema_permission_denied_throws, setup_permission_denied,
                           cleanup_permission_denied) {
    bool throw_catch = false;
    FL_TRY {
        faultline_sqlite_init_schema(t->test_db);
    }
    FL_CATCH(faultline_db_create_failed) {
        throw_catch = true; // success
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(throw_catch);
}

//  9. Existing Database Handling
static void setup_existing_database(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_existing.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Existing Database", TestSchema,
                           schema_existing_database_succeeds, setup_existing_database,
                           cleanup_test_db) {
    // Create database first time
    faultline_sqlite_init_schema(t->test_db);

    // Should succeed when called on existing database
    faultline_sqlite_init_schema(t->test_db);
}
