/**
 * @file faultline_sqlite.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-09-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline_sqlite.h>
#include <faultline/fault.h>                // for fault_buffer_count, fault_buffer...
#include <faultline/fl_context.h>           // for FLContext, faultline_get_...
#include <faultline/fl_result_codes.h>      // for faultline_result_code_to_string
#include <faultline/fl_test_summary.h>      // for FLTestSummary, faultline_...
#include <faultline/fl_exception_types.h>   // for FLExceptionReason
#include <faultline/fl_log.h>               // for LOG_DEBUG, LOG_ERROR, LOG_WARN
#include <faultline/fl_try.h>               // for FL_THROW_DETAILS, FL_CATCH_ALL
#include <sqlite/sqlite3.h>                 // for sqlite3_bind_int, sqlite3_column...
#include <stdbool.h>                        // for bool
#include <stdio.h>                          // for printf, NULL, snprintf, size_t
#include <string.h>                         // for strcpy_s, strrchr, strlen
#include <time.h>                           // for time_t
#include <faultline/fl_abbreviated_types.h> // for u32, i64
#include <faultline/fl_exception_service.h> // for FL_REASON
#include <faultline/fl_macros.h>            // for FL_UNUSED
#include <faultline/fl_test.h>              // for FLTestSuite
#include "flp_time.h"                       // for fl_gmtime

FLExceptionReason faultline_db_create_failed = "failed to create database";
FLExceptionReason faultline_db_not_found     = "database not found";

static char const *faultline_db = "Faultline DB Initialization";

// Core tables - MUST succeed on creation. These hold the raw result data the
// CLI reads back. Everything else (the derived table, indexes, and views) is
// built on top of them and is best-effort.
static char const *schema_tables[] = {
    // test_suites: registry of distinct suites, one row per suite name.
    "CREATE TABLE IF NOT EXISTS test_suites ("
    "  suite_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  suite_name TEXT NOT NULL UNIQUE,"
    "  description TEXT,"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "  last_run_at DATETIME,"
    "  total_runs INTEGER DEFAULT 0"
    ");",

    // raw_test_runs: one row per suite execution, with summary counters.
    "CREATE TABLE IF NOT EXISTS raw_test_runs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    suite_id INTEGER NOT NULL,"
    "    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
    ""
    "    test_cases INTEGER NOT NULL,"
    "    tests_run INTEGER NOT NULL,"
    "    tests_passed INTEGER NOT NULL,"
    "    tests_passed_with_leaks INTEGER NOT NULL,"
    "    tests_failed INTEGER NOT NULL,"
    "    setups_failed INTEGER NOT NULL,"
    "    cleanups_failed INTEGER NOT NULL,"
    "    total_fault_sites INTEGER NOT NULL,"
    ""
    "    discovery_failures INTEGER DEFAULT 0,"
    "    injection_failures INTEGER DEFAULT 0,"
    "    setup_failures INTEGER DEFAULT 0,"
    "    test_failures INTEGER DEFAULT 0,"
    "    cleanup_failures INTEGER DEFAULT 0,"
    "    leak_failures INTEGER DEFAULT 0,"
    "    invalid_free_failures INTEGER DEFAULT 0,"
    "    total_elapsed_time REAL NOT NULL,"
    "    average_test_time REAL,"
    "    pass_rate REAL,"
    ""
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "    FOREIGN KEY (suite_id) REFERENCES test_suites(suite_id)"
    ");",

    // raw_test_summaries: one row per test case in a run (maps FLTestSummary).
    "CREATE TABLE IF NOT EXISTS raw_test_summaries ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    run_id INTEGER NOT NULL,"
    ""
    "    test_index INTEGER NOT NULL,"
    "    test_name TEXT NOT NULL,"
    "    result_code INTEGER NOT NULL,"
    "    exception_reason TEXT,"
    "    details TEXT,"
    "    elapsed_seconds REAL NOT NULL,"
    "    faults_exercised INTEGER NOT NULL,"
    ""
    "    failure_phase INTEGER,"
    "    failure_type INTEGER,"
    "    discovery_time REAL DEFAULT 0.0,"
    "    injection_time REAL DEFAULT 0.0,"
    "    discovery_failures INTEGER DEFAULT 0,"
    "    injection_failures INTEGER DEFAULT 0,"
    ""
    "    FOREIGN KEY (run_id) REFERENCES raw_test_runs(id) ON DELETE CASCADE"
    ");",

    // raw_faults: one row per fault injected in a test case (maps Fault).
    "CREATE TABLE IF NOT EXISTS raw_faults ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    summary_id INTEGER NOT NULL,"
    ""
    "    fault_index INTEGER NOT NULL,"
    "    result_code INTEGER NOT NULL,"
    "    resource_address INTEGER,"
    "    exception_reason TEXT NOT NULL,"
    "    details TEXT NOT NULL,"
    "    source_file TEXT NOT NULL,"
    "    source_line INTEGER NOT NULL,"
    ""
    "    FOREIGN KEY (summary_id) REFERENCES raw_test_summaries(id) ON DELETE CASCADE"
    ");",

    // schema_info: records the schema version applied to this database.
    "CREATE TABLE IF NOT EXISTS schema_info ("
    "  version INTEGER PRIMARY KEY,"
    "  applied_date TEXT NOT NULL"
    ");",
    NULL // Terminator
};

// Derived tables - best-effort on creation (a failure only warns). These are
// not part of the raw result set; they are computed from it.
static char const *schema_derived_tables[] = {
    // test_case_evolution: one row per (suite, test), upserted from C in
    // faultline_record_test_summary. Holds a frozen baseline (captured the
    // first time a test is seen) alongside the latest observation, so the
    // regressions report can compare fault-site coverage and runtime against
    // the baseline. UNIQUE(suite_name, test_name) is the upsert conflict key.
    "CREATE TABLE IF NOT EXISTS test_case_evolution ("
    "    suite_name TEXT NOT NULL,"
    "    test_name TEXT NOT NULL,"
    ""
    "    first_seen_run_id INTEGER NOT NULL,"
    "    total_appearances INTEGER NOT NULL DEFAULT 1,"
    "    total_failures INTEGER NOT NULL DEFAULT 0,"
    ""
    "    baseline_run_id INTEGER NOT NULL,"
    "    baseline_fault_sites INTEGER NOT NULL,"
    "    baseline_execution_time REAL,"
    "    baseline_date TEXT,"
    ""
    "    last_run_id INTEGER NOT NULL,"
    "    last_fault_sites INTEGER NOT NULL,"
    "    last_execution_time REAL,"
    ""
    "    UNIQUE(suite_name, test_name),"
    "    FOREIGN KEY (first_seen_run_id) REFERENCES raw_test_runs(id),"
    "    FOREIGN KEY (baseline_run_id) REFERENCES raw_test_runs(id),"
    "    FOREIGN KEY (last_run_id) REFERENCES raw_test_runs(id)"
    ");",
    NULL // Terminator
};

// Performance indexes - CAN fail silently on creation
static char const *schema_indexes[] = {
    // Indexes on the raw tables for the CLI's run/summary/fault lookups.
    "CREATE INDEX IF NOT EXISTS idx_raw_test_runs_suite_timestamp ON "
    "raw_test_runs(suite_id, timestamp);",
    "CREATE INDEX IF NOT EXISTS idx_raw_test_summaries_run_index ON "
    "raw_test_summaries(run_id, test_index);",
    "CREATE INDEX IF NOT EXISTS idx_raw_faults_summary_index ON raw_faults(summary_id, "
    "fault_index);",
    NULL // Terminator
};

// Convenience views - CAN fail silently on creation
static char const *schema_views[] = {
    // latest_runs: the most recent run per suite with executed/passed counts.
    // A read-only convenience view over the live raw tables (no stored rows of
    // its own); safe to query ad hoc.
    "CREATE VIEW IF NOT EXISTS latest_runs AS "
    "SELECT "
    "    rtr.*,"
    "    ts.suite_name,"
    "    COUNT(rts.id) as executed_test_cases,"
    // result_code stores FLResultCode, where FL_PASS is 1 (FL_NOT_RUN is 0), so
    // a passing case is result_code = 1, not 0.
    "    SUM(CASE WHEN rts.result_code = 1 THEN 1 ELSE 0 END) as successful_cases "
    "FROM raw_test_runs rtr "
    "JOIN test_suites ts ON ts.suite_id = rtr.suite_id "
    "LEFT JOIN raw_test_summaries rts ON rtr.id = rts.run_id "
    "WHERE rtr.timestamp = ("
    "    SELECT MAX(timestamp) "
    "    FROM raw_test_runs rtr2 "
    "    WHERE rtr2.suite_id = rtr.suite_id"
    ")"
    "GROUP BY rtr.id;",
    NULL // Terminator
};

typedef struct {
    int         from_version;
    int         to_version;
    char const *sql;
} SchemaMigration;

/**
 * @brief Apply the full FaultLine schema to an open database connection.
 *
 * Core tables are mandatory: a failure throws faultline_db_create_failed.
 * The derived table, indexes, and views are best-effort and only log a warning
 * on failure. Records the current schema version on completion.
 *
 * @param db Open database connection. Not closed by this function.
 */
static void faultline_apply_schema(sqlite3 *db) {
    int rc;

    // Core tables - MUST succeed
    LOG_DEBUG(faultline_db, "Creating core tables...");
    for (int i = 0; schema_tables[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_tables[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            char details[256];
            snprintf(details, sizeof details, "Failed to create table %d: %s", i,
                     sqlite3_errmsg(db));
            LOG_ERROR(faultline_db, "%s", details);
            FL_THROW_DETAILS(faultline_db_create_failed, "sqlite3: %s", details);
        }
    }
    LOG_DEBUG(faultline_db, "Core tables created successfully");

    // Schema upgrades: bring an older database forward before (re)creating the
    // derived objects. test_case_evolution changed shape in v2 and the old
    // update_test_evolution trigger was retired, so drop both when upgrading
    // from a pre-v2 database; the old rows held only unread, duplicated history.
    // A fresh database has no schema_info rows yet (MAX(version) is NULL -> 0),
    // so this is a no-op for new databases.
    int stored_version = 0;
    {
        sqlite3_stmt *vstmt = NULL;
        if (sqlite3_prepare_v2(db, "SELECT MAX(version) FROM schema_info", -1, &vstmt,
                               NULL)
            == SQLITE_OK) {
            if (sqlite3_step(vstmt) == SQLITE_ROW) {
                stored_version = sqlite3_column_int(vstmt, 0);
            }
            sqlite3_finalize(vstmt);
        }
    }
    if (stored_version > 0 && stored_version < 2) {
        LOG_INFO(faultline_db,
                 "Upgrading schema from v%d: rebuilding test_case_evolution",
                 stored_version);
        sqlite3_exec(db, "DROP TRIGGER IF EXISTS update_test_evolution;", NULL, NULL,
                     NULL);
        sqlite3_exec(db, "DROP TABLE IF EXISTS test_case_evolution;", NULL, NULL, NULL);
    }

    // Derived tables - SHOULD succeed but not fatal (computed from raw data)
    LOG_DEBUG(faultline_db, "Creating derived tables...");
    for (int i = 0; schema_derived_tables[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_derived_tables[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            LOG_WARN(faultline_db, "Derived table creation failed: %s",
                     sqlite3_errmsg(db));
        }
    }

    // Indexes - CAN fail silently (performance optimization)
    LOG_DEBUG(faultline_db, "Creating indexes...");
    for (int i = 0; schema_indexes[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_indexes[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            LOG_WARN(faultline_db, "Index creation failed: %s", sqlite3_errmsg(db));
        }
    }

    // Views - CAN fail silently (convenience feature)
    LOG_DEBUG(faultline_db, "Creating views...");
    for (int i = 0; schema_views[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_views[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            LOG_WARN(faultline_db, "View creation failed: %s", sqlite3_errmsg(db));
        }
    }

    // Record schema version
    char version_sql[256];
    snprintf(version_sql, sizeof version_sql,
             "INSERT OR REPLACE INTO schema_info (version, applied_date) "
             "VALUES (%d, datetime('now'));",
             FL_SCHEMA_VERSION);
    rc = sqlite3_exec(db, version_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        LOG_WARN(faultline_db, "Schema version record failed: %s", sqlite3_errmsg(db));
    }
}

/**
 * @brief Initialize database connection and create schema if needed
 *
 * @param db_path Path to SQLite database file
 * @return sqlite3* Database connection handle, or NULL on failure
 */
sqlite3 *faultline_init_database(char const *db_path) {
    sqlite3 *db;
    LOG_VERBOSE(faultline_db, "Opening database: %s", db_path);
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             NULL);

    if (rc != SQLITE_OK) {
        char details[256];
        snprintf(details, sizeof details, "Failed to open database '%s': %s", db_path,
                 db ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
        sqlite3_close_v2(db);
        LOG_ERROR(faultline_db, "%s", details);
        FL_THROW_DETAILS(faultline_db_create_failed, "sqlite3: %s", details);
        return NULL;
    }
    LOG_DEBUG(faultline_db, "Database opened successfully");

    // enable foreign key enforcement (and ON DELETE CASCADE)
    rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        LOG_WARN(faultline_db, "Failed to enable foreign key enforcement: %s",
                 sqlite3_errmsg(db));
    }

    // Initialize schema on the opened connection
    FL_TRY {
        faultline_apply_schema(db);
        LOG_VERBOSE(faultline_db, "Database initialized successfully: %s", db_path);
    }
    FL_CATCH_ALL_RETHROW {
        LOG_ERROR(faultline_db, "Exception during database initialization");
        sqlite3_close_v2(db);
    }
    FL_END_TRY;

    return db;
}

/**
 * @brief Close database connection
 *
 * @param db Database connection handle to close
 */
void faultline_close_database(sqlite3 *db) {
    if (db != NULL) {
        int rc = sqlite3_close_v2(db);
        if (rc != SQLITE_OK) {
            LOG_WARN(faultline_db, "Warning closing database: %s", sqlite3_errmsg(db));
        }
    }
}

void faultline_sqlite_init_schema(char const *db_path) {
    // Create-and-close wrapper over faultline_init_database for callers
    // (mainly tests) that only need the schema persisted, not a live handle.
    // faultline_init_database closes and rethrows on failure, and
    // faultline_close_database is NULL-safe, so no extra guarding is needed.
    faultline_close_database(faultline_init_database(db_path));
}

/**
 * @brief Record the start of a test run and return run ID
 *
 * @param db Database connection
 * @param suite_name Name of the test suite
 * @return int Run ID for this test run, or -1 on error
 */
int faultline_record_test_run_start(sqlite3 *db, char const *suite_name,
                                    time_t start_time) {
    if (db == NULL || suite_name == NULL) {
        return -1;
    }

    int suite_id = -1;
    int run_id   = -1;

    // 0 means the caller never stamped a start time; record the current time
    // rather than 1970-01-01, which would sort the run to the bottom of every
    // timestamp-ordered report.
    if (start_time == 0) {
        time(&start_time);
    }

    struct tm  tm_result;
    struct tm *tm_info = fl_gmtime(&start_time, &tm_result);
    char       timestamp[32];
    if (tm_info) {
        // Match SQLite's CURRENT_TIMESTAMP text format (UTC, no 'T'/'Z') so rows
        // written here and rows written by the column default compare uniformly
        // in the report queries' raw string comparisons.
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    }

    FL_TRY {
        // First, ensure test suite is registered
        char const *insert_suite_sql
            = "INSERT OR IGNORE INTO test_suites (suite_name) VALUES (?);";
        sqlite3_stmt *stmt;
        int           rc = sqlite3_prepare_v2(db, insert_suite_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, suite_name, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Get suite_id
        char const *get_suite_id_sql
            = "SELECT suite_id FROM test_suites WHERE suite_name = ?;";
        rc = sqlite3_prepare_v2(db, get_suite_id_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, suite_name, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                suite_id = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        if (suite_id == -1) {
            LOG_ERROR(faultline_db, "Failed to get suite_id for suite: %s", suite_name);
        } else {
            // Create test run record
            char const *insert_run_sql
                = "INSERT INTO raw_test_runs ("
                  "    suite_id, timestamp, test_cases, tests_run, tests_passed, "
                  "    tests_passed_with_leaks, tests_failed, setups_failed, "
                  "    cleanups_failed, total_fault_sites, total_elapsed_time"
                  ") VALUES (?, ?, 0, 0, 0, 0, 0, 0, 0, 0, 0.0);";

            rc = sqlite3_prepare_v2(db, insert_run_sql, -1, &stmt, NULL);
            if (rc == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, suite_id);
                // When tm_info is NULL (unreachable in practice), passing NULL to
                // sqlite3_bind_text stores SQL NULL rather than triggering the column
                // default. That's acceptable for an edge case that can't happen at
                // runtime.
                sqlite3_bind_text(stmt, 2, tm_info ? timestamp : NULL, -1,
                                  SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    run_id = (int)sqlite3_last_insert_rowid(db);
                    LOG_VERBOSE(faultline_db, "Started test run %d for suite: %s",
                                run_id, suite_name);
                }
                sqlite3_finalize(stmt);
            }
        }
    }
    FL_CATCH_ALL {
        LOG_ERROR(faultline_db, "Failed to record test run start: %s", FL_REASON);
    }
    FL_END_TRY;

    return run_id;
}

/**
 * @brief Complete a test run with final statistics
 *
 * @param db Database connection
 * @param run_id Run ID returned from faultline_record_test_run_start()
 * @param fctx FLContext containing final test results
 */
void faultline_record_test_run_complete(sqlite3 *db, int run_id, FLContext *fctx) {
    if (db == NULL || run_id <= 0 || fctx == NULL) {
        return;
    }

    FL_TRY {
        // Calculate totals from FLContext
        size_t results_count    = faultline_get_results_count(fctx);
        size_t tests_run        = faultline_get_run_count(fctx);
        size_t tests_passed     = faultline_get_pass_count(fctx);
        size_t setup_failures   = faultline_get_setup_fail_count(fctx);
        size_t test_failures    = faultline_get_fail_count(fctx);
        size_t cleanup_failures = faultline_get_cleanup_fail_count(fctx);

        i64    total_fault_sites     = 0;
        double total_elapsed_time    = 0.0;
        u32    discovery_failures    = 0;
        u32    injection_failures    = 0;
        u32    leak_failures         = 0;
        u32    invalid_free_failures = 0;

        // Calculate aggregates from detailed results
        for (u32 i = 0; i < results_count; i++) {
            FLTestSummary *summary
                = faultline_test_summary_buffer_get(&fctx->results, i);
            total_fault_sites += summary->faults_exercised;
            total_elapsed_time += summary->elapsed_seconds;
            discovery_failures += summary->discovery_failures;
            injection_failures += summary->injection_failures;

            if (summary->code == FL_LEAK) {
                leak_failures++;
            }
            if (summary->code == FL_INVALID_FREE || summary->code == FL_DOUBLE_FREE) {
                invalid_free_failures++;
            }
        }

        // Parameter indices for UPDATE raw_test_runs
        enum {
            PARAM_TEST_CASES = 1,
            PARAM_TESTS_RUN,
            PARAM_TESTS_PASSED,
            PARAM_TESTS_PASSED_WITH_LEAKS,
            PARAM_TESTS_FAILED,
            PARAM_SETUPS_FAILED,
            PARAM_CLEANUPS_FAILED,
            PARAM_TOTAL_FAULT_SITES,
            PARAM_DISCOVERY_FAILURES,
            PARAM_INJECTION_FAILURES,
            PARAM_SETUP_FAILURES,
            PARAM_TEST_FAILURES,
            PARAM_CLEANUP_FAILURES,
            PARAM_LEAK_FAILURES,
            PARAM_INVALID_FREE_FAILURES,
            PARAM_TOTAL_ELAPSED_TIME,
            PARAM_AVERAGE_TEST_TIME,
            PARAM_PASS_RATE,
            PARAM_RUN_ID
        };

        // Update the test run record with final statistics
        char const *update_sql
            = "UPDATE raw_test_runs SET "
              "    test_cases = ?, tests_run = ?, tests_passed = ?, "
              "    tests_passed_with_leaks = ?, tests_failed = ?, "
              "    setups_failed = ?, cleanups_failed = ?, total_fault_sites = ?, "
              "    discovery_failures = ?, injection_failures = ?, "
              "    setup_failures = ?, test_failures = ?, cleanup_failures = ?, "
              "    leak_failures = ?, invalid_free_failures = ?, "
              "    total_elapsed_time = ?, average_test_time = ?, pass_rate = ? "
              "WHERE id = ?;";

        sqlite3_stmt *stmt;
        int           rc = sqlite3_prepare_v2(db, update_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, PARAM_TEST_CASES, (int)fctx->ts->count);
            sqlite3_bind_int(stmt, PARAM_TESTS_RUN, (int)tests_run);
            sqlite3_bind_int(stmt, PARAM_TESTS_PASSED, (int)tests_passed);
            sqlite3_bind_int(stmt, PARAM_TESTS_PASSED_WITH_LEAKS,
                             (int)faultline_get_pass_with_leaks_count(fctx));
            sqlite3_bind_int(stmt, PARAM_TESTS_FAILED, (int)test_failures);
            sqlite3_bind_int(stmt, PARAM_SETUPS_FAILED, (int)setup_failures);
            sqlite3_bind_int(stmt, PARAM_CLEANUPS_FAILED, (int)cleanup_failures);
            sqlite3_bind_int64(stmt, PARAM_TOTAL_FAULT_SITES, total_fault_sites);
            sqlite3_bind_int(stmt, PARAM_DISCOVERY_FAILURES, discovery_failures);
            sqlite3_bind_int(stmt, PARAM_INJECTION_FAILURES, injection_failures);
            sqlite3_bind_int(stmt, PARAM_SETUP_FAILURES, (int)setup_failures);
            sqlite3_bind_int(stmt, PARAM_TEST_FAILURES, (int)test_failures);
            sqlite3_bind_int(stmt, PARAM_CLEANUP_FAILURES, (int)cleanup_failures);
            sqlite3_bind_int(stmt, PARAM_LEAK_FAILURES, leak_failures);
            sqlite3_bind_int(stmt, PARAM_INVALID_FREE_FAILURES, invalid_free_failures);
            sqlite3_bind_double(stmt, PARAM_TOTAL_ELAPSED_TIME, total_elapsed_time);
            sqlite3_bind_double(stmt, PARAM_AVERAGE_TEST_TIME,
                                tests_run > 0 ? total_elapsed_time / tests_run : 0.0);
            sqlite3_bind_double(stmt, PARAM_PASS_RATE,
                                tests_run > 0 ? (double)tests_passed / tests_run : 0.0);
            sqlite3_bind_int(stmt, PARAM_RUN_ID, run_id);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                LOG_VERBOSE(faultline_db, "Completed test run %d: %zu/%zu tests passed",
                            run_id, tests_passed, tests_run);
            }
            sqlite3_finalize(stmt);
        }
    }
    FL_CATCH_ALL {
        LOG_ERROR(faultline_db, "Failed to complete test run %d: %s", run_id, FL_REASON);
    }
    FL_END_TRY;
}

/**
 * @brief Upsert the per-(suite, test) evolution row from a recorded summary.
 *
 * The baseline columns are captured once, when the row is first inserted, and
 * left untouched on conflict so they stay frozen at the test's first sighting;
 * the last_* columns and the appearance/failure counters track the most recent
 * run. Best-effort: a failure only warns. The suite name is resolved from
 * run_id so callers need not pass it.
 *
 * @param db Database connection
 * @param run_id Run ID the summary belongs to
 * @param summary Test summary data (uses code, faults_exercised, elapsed_seconds)
 * @param test_name Name of the test case
 */
static void faultline_update_test_evolution(sqlite3 *db, int run_id,
                                            FLTestSummary const *summary,
                                            char const          *test_name) {
    char const *upsert_sql
        = "INSERT INTO test_case_evolution ("
          "    suite_name, test_name, first_seen_run_id, total_appearances, "
          "total_failures, "
          "    baseline_run_id, baseline_fault_sites, baseline_execution_time, "
          "baseline_date, "
          "    last_run_id, last_fault_sites, last_execution_time"
          ") VALUES ("
          "    (SELECT ts.suite_name FROM raw_test_runs r "
          "       JOIN test_suites ts ON ts.suite_id = r.suite_id WHERE r.id = ?1),"
          "    ?2, ?1, 1, ?3,"
          "    ?1, ?4, ?5, datetime('now'),"
          "    ?1, ?4, ?5"
          ") "
          "ON CONFLICT(suite_name, test_name) DO UPDATE SET "
          "    total_appearances   = total_appearances + 1,"
          "    total_failures      = total_failures + excluded.total_failures,"
          "    last_run_id         = excluded.last_run_id,"
          "    last_fault_sites    = excluded.last_fault_sites,"
          "    last_execution_time = excluded.last_execution_time;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, upsert_sql, -1, &stmt, NULL) != SQLITE_OK) {
        LOG_WARN(faultline_db, "test_case_evolution upsert prepare failed: %s",
                 sqlite3_errmsg(db));
        return;
    }

    // Enum is ordered best->worst (FL_NOT_RUN, FL_PASS, then failures), matching
    // the result_code > FL_PASS "failed" convention used elsewhere in this file.
    int failed = (summary->code > FL_PASS) ? 1 : 0;
    sqlite3_bind_int(stmt, 1, run_id);
    sqlite3_bind_text(stmt, 2, test_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, failed);
    sqlite3_bind_int64(stmt, 4, summary->faults_exercised);
    sqlite3_bind_double(stmt, 5, summary->elapsed_seconds);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LOG_WARN(faultline_db, "test_case_evolution upsert failed: %s",
                 sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Record a single test summary result
 *
 * @param db Database connection
 * @param run_id Run ID from faultline_record_test_run_start()
 * @param summary Test summary data
 * @param test_name Name of the test case
 */
void faultline_record_test_summary(sqlite3 *db, int run_id, FLTestSummary *summary,
                                   char const *test_name) {
    if (db == NULL || run_id <= 0 || summary == NULL || test_name == NULL) {
        return;
    }

    FL_TRY {
        // Parameter indices for INSERT INTO raw_test_summaries
        enum {
            PARAM_RUN_ID = 1,
            PARAM_TEST_INDEX,
            PARAM_TEST_NAME,
            PARAM_RESULT_CODE,
            PARAM_EXCEPTION_REASON,
            PARAM_DETAILS,
            PARAM_ELAPSED_SECONDS,
            PARAM_FAULTS_EXERCISED,
            PARAM_FAILURE_PHASE,
            PARAM_FAILURE_TYPE,
            PARAM_DISCOVERY_TIME,
            PARAM_INJECTION_TIME,
            PARAM_DISCOVERY_FAILURES,
            PARAM_INJECTION_FAILURES
        };

        char const *insert_sql
            = "INSERT INTO raw_test_summaries ("
              "    run_id, test_index, test_name, result_code, exception_reason, "
              "    details, elapsed_seconds, faults_exercised, failure_phase, "
              "failure_type, "
              "    discovery_time, injection_time, discovery_failures, "
              "injection_failures"
              ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sqlite3_stmt *stmt;
        int           rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, PARAM_RUN_ID, run_id);
            sqlite3_bind_int(stmt, PARAM_TEST_INDEX, (int)summary->index);
            sqlite3_bind_text(stmt, PARAM_TEST_NAME, test_name, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, PARAM_RESULT_CODE, (int)summary->code);
            sqlite3_bind_text(stmt, PARAM_EXCEPTION_REASON,
                              summary->reason ? summary->reason : "", -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, PARAM_DETAILS,
                              summary->details ? summary->details : "", -1,
                              SQLITE_STATIC);
            sqlite3_bind_double(stmt, PARAM_ELAPSED_SECONDS, summary->elapsed_seconds);
            sqlite3_bind_int64(stmt, PARAM_FAULTS_EXERCISED, summary->faults_exercised);
            sqlite3_bind_int(stmt, PARAM_FAILURE_PHASE, (int)summary->failure_phase);
            sqlite3_bind_int(stmt, PARAM_FAILURE_TYPE, (int)summary->failure_type);
            sqlite3_bind_double(stmt, PARAM_DISCOVERY_TIME, summary->discovery_time);
            sqlite3_bind_double(stmt, PARAM_INJECTION_TIME, summary->injection_time);
            sqlite3_bind_int(stmt, PARAM_DISCOVERY_FAILURES,
                             summary->discovery_failures);
            sqlite3_bind_int(stmt, PARAM_INJECTION_FAILURES,
                             summary->injection_failures);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                int summary_id = (int)sqlite3_last_insert_rowid(db);

                // Record individual fault data
                size_t fault_count = fault_buffer_count(&summary->fault_buffer);
                LOG_DEBUG(faultline_db, "Test %s: fault_count=%zu, result_code=%s",
                          test_name, fault_count,
                          faultline_result_code_to_string(summary->code));
                if (fault_count == 0) {
                    LOG_DEBUG(faultline_db,
                              "No faults found in fault_buffer for test %s (result=%s)",
                              test_name, faultline_result_code_to_string(summary->code));
                } else {
                    LOG_DEBUG(faultline_db,
                              "Processing %zu faults for test summary %d (%s)",
                              fault_count, summary_id, test_name);
                }

                // Parameter indices for INSERT INTO raw_faults
                enum {
                    FAULT_PARAM_SUMMARY_ID = 1,
                    FAULT_PARAM_FAULT_INDEX,
                    FAULT_PARAM_RESULT_CODE,
                    FAULT_PARAM_RESOURCE_ADDRESS,
                    FAULT_PARAM_EXCEPTION_REASON,
                    FAULT_PARAM_DETAILS,
                    FAULT_PARAM_SOURCE_FILE,
                    FAULT_PARAM_SOURCE_LINE
                };

                for (size_t f = 0; f < fault_count; f++) {
                    Fault *fault = fault_buffer_get(&summary->fault_buffer, f);
                    if (fault != NULL) {
                        LOG_DEBUG(faultline_db,
                                  "Fault %zu: file=%s, line=%d, reason=%s, details=%s, "
                                  "resource=%p",
                                  f, fault->file ? fault->file : "NULL", fault->line,
                                  fault->reason ? fault->reason : "NULL",
                                  fault->details ? fault->details : "NULL",
                                  (void *)fault->resource);
                        char const *fault_insert_sql
                            = "INSERT INTO raw_faults ("
                              "    summary_id, fault_index, result_code, "
                              "resource_address, "
                              "    exception_reason, details, source_file, source_line"
                              ") VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

                        sqlite3_stmt *fault_stmt;
                        int fault_rc = sqlite3_prepare_v2(db, fault_insert_sql, -1,
                                                          &fault_stmt, NULL);
                        if (fault_rc == SQLITE_OK) {
                            sqlite3_bind_int(fault_stmt, FAULT_PARAM_SUMMARY_ID,
                                             summary_id);
                            sqlite3_bind_int64(fault_stmt, FAULT_PARAM_FAULT_INDEX,
                                               fault->index);
                            sqlite3_bind_int(fault_stmt, FAULT_PARAM_RESULT_CODE,
                                             (int)fault->code);
                            sqlite3_bind_int64(fault_stmt, FAULT_PARAM_RESOURCE_ADDRESS,
                                               (sqlite3_int64)fault->resource);
                            sqlite3_bind_text(fault_stmt, FAULT_PARAM_EXCEPTION_REASON,
                                              fault->reason ? fault->reason : "", -1,
                                              SQLITE_STATIC);
                            sqlite3_bind_text(fault_stmt, FAULT_PARAM_DETAILS,
                                              fault->details ? fault->details : "", -1,
                                              SQLITE_STATIC);
                            sqlite3_bind_text(fault_stmt, FAULT_PARAM_SOURCE_FILE,
                                              fault->file ? fault->file : "", -1,
                                              SQLITE_STATIC);
                            sqlite3_bind_int(fault_stmt, FAULT_PARAM_SOURCE_LINE,
                                             fault->line);

                            if (sqlite3_step(fault_stmt) == SQLITE_DONE) {
                                LOG_DEBUG(faultline_db,
                                          "Recorded fault %zu for test summary %d", f,
                                          summary_id);
                            }
                            sqlite3_finalize(fault_stmt);
                        }
                    }
                }

                LOG_DEBUG(faultline_db,
                          "Recorded test summary %d for test: %s with %zu faults",
                          summary_id, test_name, fault_count);

                // Maintain the per-(suite, test) baseline/latest rollup.
                faultline_update_test_evolution(db, run_id, summary, test_name);
            }
            sqlite3_finalize(stmt);
        }
    }
    FL_CATCH_ALL {
        LOG_ERROR(faultline_db, "Failed to record test summary for %s: %s", test_name,
                  FL_REASON);
    }
    FL_END_TRY;
}

/**
 * @brief Show recent test runs with summary statistics
 *
 * @param db Database connection
 * @param limit Maximum number of runs to display (0 = no limit)
 */
void faultline_show_recent_runs(sqlite3 *db, int limit) {
    if (db == NULL) {
        printf("No database connection available\n");
        return;
    }

    char const *sql = "SELECT rtr.id, ts.suite_name, rtr.timestamp, rtr.test_cases, "
                      "       rtr.tests_run, rtr.tests_passed, rtr.total_elapsed_time, "
                      "rtr.pass_rate, "
                      "       rtr.total_fault_sites "
                      "FROM raw_test_runs rtr "
                      "JOIN test_suites ts ON rtr.suite_id = ts.suite_id "
                      "ORDER BY rtr.timestamp DESC";

    char query[512];
    if (limit > 0) {
        snprintf(query, sizeof query, "%s LIMIT %d;", sql, limit);
    } else {
        snprintf(query, sizeof query, "%s;", sql);
    }

    sqlite3_stmt *stmt;
    int           rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing query: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Column indices for recent runs query
    enum {
        COL_ID = 0,
        COL_SUITE_NAME,
        COL_TIMESTAMP,
        COL_TEST_CASES,
        COL_TESTS_RUN,
        COL_TESTS_PASSED,
        COL_ELAPSED_TIME,
        COL_PASS_RATE,
        COL_FAULT_SITES
    };

    printf("\n=== Recent Test Runs ===\n");
    printf("%-4s %-20s %-19s %-6s %-7s %-4s/%-4s %-8s %-6s\n", "ID", "Suite",
           "Timestamp", "Cases", "Faults", "Pass", "Run", "Time(s)", "Rate%");
    printf("%-4s %-20s %-19s %-6s %-7s %-9s %-8s %-6s\n", "----", "--------------------",
           "-------------------", "------", "-------", "---------", "--------",
           "------");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int         id         = sqlite3_column_int(stmt, COL_ID);
        char const *suite_name = (char const *)sqlite3_column_text(stmt, COL_SUITE_NAME);
        char const *timestamp  = (char const *)sqlite3_column_text(stmt, COL_TIMESTAMP);

        // A NULL column (SQL NULL or an OOM inside sqlite3_column_text) must not
        // reach strncpy_s/strlen, which abort on NULL under MSVC.
        if (suite_name == NULL) {
            suite_name = "(null)";
        }
        if (timestamp == NULL) {
            timestamp = "(null)";
        }
        int    test_cases   = sqlite3_column_int(stmt, COL_TEST_CASES);
        int    tests_run    = sqlite3_column_int(stmt, COL_TESTS_RUN);
        int    tests_passed = sqlite3_column_int(stmt, COL_TESTS_PASSED);
        double elapsed_time = sqlite3_column_double(stmt, COL_ELAPSED_TIME);
        double pass_rate    = sqlite3_column_double(stmt, COL_PASS_RATE);
        int    fault_sites  = sqlite3_column_int(stmt, COL_FAULT_SITES);

        // Truncate timestamp to remove seconds
        char short_timestamp[20];
        strncpy_s(short_timestamp, sizeof short_timestamp, timestamp, 16);
        short_timestamp[16] = '\0';

        // Truncate suite name if too long to maintain table alignment
        char truncated_suite[21]; // 20 chars + null terminator
        if (strlen(suite_name) > 20) {
            strncpy_s(truncated_suite, sizeof truncated_suite, suite_name, 17);
            truncated_suite[17] = '.';
            truncated_suite[18] = '.';
            truncated_suite[19] = '.';
            truncated_suite[20] = '\0';
        } else {
            strcpy_s(truncated_suite, sizeof truncated_suite, suite_name);
        }

        printf("%-4d %-20s %-19s %-6d %-7d %-4d/%-4d %-8.3f %5.1f%%\n", id,
               truncated_suite, short_timestamp, test_cases, fault_sites, tests_passed,
               tests_run, elapsed_time, pass_rate * 100.0);
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Prepare a suite-filtered report query with an optional row limit.
 *
 * Appends "LIMIT n" when limit > 0, prepares base_sql, and binds ?1 to the suite filter
 * (NULL binds SQL NULL, which the report queries treat as "all suites"). Callers bind
 * any further parameters (e.g. ?2) on the returned statement.
 *
 * @return Prepared statement, or NULL on a prepare error.
 */
static sqlite3_stmt *prepare_suite_report(sqlite3 *db, char const *base_sql,
                                          char const *suite_name, int limit) {
    char query[1024];
    if (limit > 0) {
        snprintf(query, sizeof query, "%s LIMIT %d;", base_sql, limit);
    } else {
        snprintf(query, sizeof query, "%s;", base_sql);
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        LOG_WARN(faultline_db, "report query prepare failed: %s", sqlite3_errmsg(db));
        return NULL;
    }

    if (suite_name != NULL) {
        sqlite3_bind_text(stmt, 1, suite_name, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 1);
    }

    return stmt;
}

/**
 * @brief Iterate tests whose coverage or runtime regressed against their baseline
 *
 * Reads test_case_evolution and invokes `fn` for each test where the latest run
 * lost fault-site coverage (last_fault_sites < baseline_fault_sites) or got
 * slower than the baseline by more than threshold_pct. Fault-site coverage is
 * the robust signal: for an unchanged test it is deterministic, so a drop points
 * at the system under test or at fault discovery. Runtime is noisier, hence the
 * threshold. The coverage_regression/runtime_regression flags on each row record
 * which condition fired. Row string fields are only valid during the callback.
 *
 * @param db Database connection
 * @param suite_name Suite to filter by, or NULL for all suites
 * @param limit Maximum rows to visit (0 = no limit)
 * @param threshold_pct Runtime regression threshold, in percent (e.g. 20.0)
 * @param fn Callback invoked once per regression
 * @param ctx Opaque pointer passed through to `fn`
 * @return Number of regressions visited, or -1 on a query error
 */
int faultline_for_each_regression(sqlite3 *db, char const *suite_name, int limit,
                                  double threshold_pct, FLRegressionFn fn, void *ctx) {
    if (db == NULL || fn == NULL) {
        return -1;
    }

    double threshold_frac = threshold_pct / 100.0;

    char const *base_sql
        = "SELECT suite_name, test_name, baseline_fault_sites, last_fault_sites, "
          "       baseline_execution_time, last_execution_time "
          "FROM test_case_evolution "
          "WHERE (?1 IS NULL OR suite_name = ?1) "
          "  AND (last_fault_sites < baseline_fault_sites "
          "       OR (baseline_execution_time > 0 "
          "           AND last_execution_time > baseline_execution_time * (1.0 + ?2))) "
          "ORDER BY (CAST(baseline_fault_sites - last_fault_sites AS REAL) / "
          "          CASE WHEN baseline_fault_sites = 0 THEN 1 "
          "               ELSE baseline_fault_sites END) DESC, suite_name, test_name";

    sqlite3_stmt *stmt = prepare_suite_report(db, base_sql, suite_name, limit);
    if (stmt == NULL) {
        return -1;
    }
    sqlite3_bind_double(stmt, 2, threshold_frac);

    enum {
        COL_SUITE = 0,
        COL_TEST,
        COL_BASE_SITES,
        COL_LAST_SITES,
        COL_BASE_TIME,
        COL_LAST_TIME
    };

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FLRegression r;
        r.suite_name              = (char const *)sqlite3_column_text(stmt, COL_SUITE);
        r.test_name               = (char const *)sqlite3_column_text(stmt, COL_TEST);
        r.baseline_fault_sites    = sqlite3_column_int64(stmt, COL_BASE_SITES);
        r.last_fault_sites        = sqlite3_column_int64(stmt, COL_LAST_SITES);
        r.baseline_execution_time = sqlite3_column_double(stmt, COL_BASE_TIME);
        r.last_execution_time     = sqlite3_column_double(stmt, COL_LAST_TIME);
        r.coverage_regression     = r.last_fault_sites < r.baseline_fault_sites;
        r.runtime_regression = r.baseline_execution_time > 0.0
                               && r.last_execution_time > r.baseline_execution_time
                                                              * (1.0 + threshold_frac);

        fn(&r, ctx);
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

// Print one regression row in the "show regressions" table.
static void print_regression_row(FLRegression const *r, void *ctx) {
    (void)ctx;

    double      fs_delta = (r->baseline_fault_sites != 0)
                               ? ((double)(r->last_fault_sites - r->baseline_fault_sites)
                                  / (double)r->baseline_fault_sites)
                                     * 100.0
                               : 0.0;
    double      t_delta  = (r->baseline_execution_time > 0.0)
                               ? ((r->last_execution_time - r->baseline_execution_time)
                                  / r->baseline_execution_time)
                                     * 100.0
                               : 0.0;
    char const *type     = (r->coverage_regression && r->runtime_regression)
                               ? "COVERAGE+RUNTIME"
                           : r->coverage_regression ? "COVERAGE"
                                                    : "RUNTIME";

    printf("%-20.20s %-24.24s %7lld %7lld %7.1f%%   %9.3f %9.3f %7.1f%%  %s\n",
           r->suite_name, r->test_name, (long long)r->baseline_fault_sites,
           (long long)r->last_fault_sites, fs_delta, r->baseline_execution_time,
           r->last_execution_time, t_delta, type);
}

/**
 * @brief Show tests whose coverage or runtime regressed against their baseline
 *
 * Thin presentation wrapper over faultline_for_each_regression; see that
 * function for the selection semantics.
 *
 * @param db Database connection
 * @param suite_name Suite to filter by, or NULL for all suites
 * @param limit Maximum rows to display (0 = no limit)
 * @param threshold_pct Runtime regression threshold, in percent (e.g. 20.0)
 */
void faultline_show_regressions(sqlite3 *db, char const *suite_name, int limit,
                                double threshold_pct) {
    if (db == NULL) {
        printf("No database connection available\n");
        return;
    }

    printf("\n=== Regressions vs Baseline (runtime threshold %.0f%%) ===\n",
           threshold_pct);
    printf("%-20s %-24s %7s %7s %8s   %9s %9s %8s  %s\n", "Suite", "Test", "BaseFS",
           "LastFS", "FS d%", "BaseT(s)", "LastT(s)", "T d%", "Type");
    printf("%-20s %-24s %7s %7s %8s   %9s %9s %8s  %s\n", "--------------------",
           "------------------------", "-------", "-------", "--------", "---------",
           "---------", "--------", "----");

    int count = faultline_for_each_regression(db, suite_name, limit, threshold_pct,
                                              print_regression_row, NULL);
    if (count < 0) {
        printf("Error querying regressions\n");
    } else if (count == 0) {
        printf("(no regressions)\n");
    }
}

/**
 * @brief Re-pin regression baselines to each test's latest observation.
 *
 * Sets the baseline_* columns of test_case_evolution equal to the last_* values
 * and stamps baseline_date, so subsequent `show regressions` compares against
 * the current state rather than the original first-sighting baseline. Scoped by
 * the optional suite and test filters (NULL matches all).
 *
 * @param db Database connection
 * @param suite_name Suite to scope to, or NULL for all suites
 * @param test_name Test case to scope to, or NULL for all tests
 * @return Number of baselines re-pinned, or -1 on a query error
 */
int faultline_reset_baselines(sqlite3 *db, char const *suite_name,
                              char const *test_name) {
    if (db == NULL) {
        return -1;
    }

    char const *sql = "UPDATE test_case_evolution SET "
                      "    baseline_run_id = last_run_id, "
                      "    baseline_fault_sites = last_fault_sites, "
                      "    baseline_execution_time = last_execution_time, "
                      "    baseline_date = datetime('now') "
                      "WHERE (?1 IS NULL OR suite_name = ?1) "
                      "  AND (?2 IS NULL OR test_name = ?2);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        LOG_WARN(faultline_db, "baseline reset prepare failed: %s", sqlite3_errmsg(db));
        return -1;
    }

    if (suite_name != NULL) {
        sqlite3_bind_text(stmt, 1, suite_name, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    if (test_name != NULL) {
        sqlite3_bind_text(stmt, 2, test_name, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 2);
    }

    int updated = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        updated = sqlite3_changes(db);
    } else {
        LOG_WARN(faultline_db, "baseline reset failed: %s", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return updated;
}

/**
 * @brief Iterate per-suite, per-day health metrics with the prior day alongside
 *
 * Aggregates raw_test_runs by suite and calendar day (average pass rate and run
 * time, summed failures, run count), and uses LAG window functions to attach
 * each day's previous-day values so callers can show trend direction. The
 * metrics are computed directly from the raw runs, so no rollup table is
 * required. Rows are visited most-recent-day first. The has_prev flag is false
 * for a suite's earliest day, where the previous-day values are undefined.
 * Row string fields are only valid during the callback.
 *
 * @param db Database connection
 * @param suite_name Suite to filter by, or NULL for all suites
 * @param limit Maximum rows to visit (0 = no limit)
 * @param fn Callback invoked once per suite-day
 * @param ctx Opaque pointer passed through to `fn`
 * @return Number of rows visited, or -1 on a query error
 */
int faultline_for_each_trend(sqlite3 *db, char const *suite_name, int limit,
                             FLSuiteTrendFn fn, void *ctx) {
    if (db == NULL || fn == NULL) {
        return -1;
    }

    char const *base_sql
        = "SELECT suite_name, day, total_runs, avg_pass_rate, avg_execution_time, "
          "       total_failures, "
          "       LAG(avg_pass_rate)      OVER w AS prev_pass_rate, "
          "       LAG(avg_execution_time) OVER w AS prev_execution_time, "
          "       LAG(total_failures)     OVER w AS prev_total_failures "
          "FROM ("
          "    SELECT ts.suite_name AS suite_name, date(rtr.timestamp) AS day, "
          "           COUNT(*) AS total_runs, "
          "           AVG(rtr.pass_rate) AS avg_pass_rate, "
          "           AVG(rtr.total_elapsed_time) AS avg_execution_time, "
          "           SUM(rtr.tests_failed) AS total_failures "
          "    FROM raw_test_runs rtr "
          "    JOIN test_suites ts ON ts.suite_id = rtr.suite_id "
          "    WHERE (?1 IS NULL OR ts.suite_name = ?1) "
          "    GROUP BY ts.suite_name, day"
          ") "
          "WINDOW w AS (PARTITION BY suite_name ORDER BY day) "
          "ORDER BY suite_name, day DESC";

    sqlite3_stmt *stmt = prepare_suite_report(db, base_sql, suite_name, limit);
    if (stmt == NULL) {
        return -1;
    }

    enum {
        COL_SUITE = 0,
        COL_DAY,
        COL_RUNS,
        COL_PASS,
        COL_EXEC,
        COL_FAILS,
        COL_PREV_PASS,
        COL_PREV_EXEC,
        COL_PREV_FAILS
    };

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FLSuiteTrend tr;
        tr.suite_name          = (char const *)sqlite3_column_text(stmt, COL_SUITE);
        tr.day                 = (char const *)sqlite3_column_text(stmt, COL_DAY);
        tr.total_runs          = sqlite3_column_int(stmt, COL_RUNS);
        tr.avg_pass_rate       = sqlite3_column_double(stmt, COL_PASS);
        tr.avg_execution_time  = sqlite3_column_double(stmt, COL_EXEC);
        tr.total_failures      = sqlite3_column_int64(stmt, COL_FAILS);
        tr.has_prev            = sqlite3_column_type(stmt, COL_PREV_PASS) != SQLITE_NULL;
        tr.prev_pass_rate      = sqlite3_column_double(stmt, COL_PREV_PASS);
        tr.prev_execution_time = sqlite3_column_double(stmt, COL_PREV_EXEC);
        tr.prev_total_failures = sqlite3_column_int64(stmt, COL_PREV_FAILS);

        fn(&tr, ctx);
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

// Print one suite-day row in the "show trends" table, with deltas vs the prior
// day (or "-" on a suite's first day).
static void print_trend_row(FLSuiteTrend const *t, void *ctx) {
    (void)ctx;

    char dpass[16];
    char dtime[16];
    char dfail[16];
    if (t->has_prev) {
        snprintf(dpass, sizeof dpass, "%+.1f",
                 (t->avg_pass_rate - t->prev_pass_rate) * 100.0);
        snprintf(dtime, sizeof dtime, "%+.3f",
                 t->avg_execution_time - t->prev_execution_time);
        snprintf(dfail, sizeof dfail, "%+lld",
                 (long long)(t->total_failures - t->prev_total_failures));
    } else {
        snprintf(dpass, sizeof dpass, "%s", "-");
        snprintf(dtime, sizeof dtime, "%s", "-");
        snprintf(dfail, sizeof dfail, "%s", "-");
    }

    printf("%-20.20s %-10s %5d  %6.1f%% %8s  %8.3f %8s  %6lld %6s\n", t->suite_name,
           t->day, t->total_runs, t->avg_pass_rate * 100.0, dpass, t->avg_execution_time,
           dtime, (long long)t->total_failures, dfail);
}

/**
 * @brief Show per-suite health trends over time
 *
 * Thin presentation wrapper over faultline_for_each_trend; see that function for
 * the aggregation semantics.
 *
 * @param db Database connection
 * @param suite_name Suite to filter by, or NULL for all suites
 * @param limit Maximum rows to display (0 = no limit)
 */
void faultline_show_trends(sqlite3 *db, char const *suite_name, int limit) {
    if (db == NULL) {
        printf("No database connection available\n");
        return;
    }

    printf("\n=== Suite Trends (per day) ===\n");
    printf("%-20s %-10s %5s  %7s %8s  %8s %8s  %6s %6s\n", "Suite", "Day", "Runs",
           "Pass%", "Pass d", "Time(s)", "Time d", "Fails", "Fail d");
    printf("%-20s %-10s %5s  %7s %8s  %8s %8s  %6s %6s\n", "--------------------",
           "----------", "-----", "-------", "--------", "--------", "--------",
           "------", "------");

    int count = faultline_for_each_trend(db, suite_name, limit, print_trend_row, NULL);
    if (count < 0) {
        printf("Error querying trends\n");
    } else if (count == 0) {
        printf("(no trend data)\n");
    }
}

/**
 * @brief Iterate the source locations that produced the most faults
 *
 * Groups raw_faults by (source_file, source_line) and reports, per location,
 * how many faults occurred and how many distinct test cases were affected,
 * worst first. Computed directly from the recorded faults, so no rollup table
 * is required. Row string fields are only valid during the callback.
 *
 * @param db Database connection
 * @param suite_name Suite to filter by, or NULL for all suites
 * @param limit Maximum rows to visit (0 = no limit)
 * @param fn Callback invoked once per location
 * @param ctx Opaque pointer passed through to `fn`
 * @return Number of rows visited, or -1 on a query error
 */
int faultline_for_each_hotspot(sqlite3 *db, char const *suite_name, int limit,
                               FLFaultHotspotFn fn, void *ctx) {
    if (db == NULL || fn == NULL) {
        return -1;
    }

    char const *base_sql
        = "SELECT rf.source_file, rf.source_line, COUNT(*) AS failure_count, "
          "       COUNT(DISTINCT rf.summary_id) AS tests_affected "
          "FROM raw_faults rf "
          "JOIN raw_test_summaries rts ON rts.id = rf.summary_id "
          "JOIN raw_test_runs rtr ON rtr.id = rts.run_id "
          "JOIN test_suites ts ON ts.suite_id = rtr.suite_id "
          "WHERE (?1 IS NULL OR ts.suite_name = ?1) "
          "GROUP BY rf.source_file, rf.source_line "
          "ORDER BY failure_count DESC, rf.source_file, rf.source_line";

    sqlite3_stmt *stmt = prepare_suite_report(db, base_sql, suite_name, limit);
    if (stmt == NULL) {
        return -1;
    }

    enum {
        COL_FILE = 0,
        COL_LINE,
        COL_FAILURES,
        COL_TESTS
    };

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FLFaultHotspot h;
        h.source_file    = (char const *)sqlite3_column_text(stmt, COL_FILE);
        h.source_line    = sqlite3_column_int(stmt, COL_LINE);
        h.failure_count  = sqlite3_column_int64(stmt, COL_FAILURES);
        h.tests_affected = sqlite3_column_int64(stmt, COL_TESTS);

        fn(&h, ctx);
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

// Print one hotspot row in the "show hotspots" table.
static void print_hotspot_row(FLFaultHotspot const *h, void *ctx) {
    (void)ctx;
    printf("%6lld %6lld  %s:%d\n", (long long)h->failure_count,
           (long long)h->tests_affected, h->source_file, h->source_line);
}

/**
 * @brief Show the source locations that produced the most faults
 *
 * Thin presentation wrapper over faultline_for_each_hotspot; see that function
 * for the aggregation semantics.
 *
 * @param db Database connection
 * @param suite_name Suite to filter by, or NULL for all suites
 * @param limit Maximum rows to display (0 = no limit)
 */
void faultline_show_hotspots(sqlite3 *db, char const *suite_name, int limit) {
    if (db == NULL) {
        printf("No database connection available\n");
        return;
    }

    printf("\n=== Fault Hotspots ===\n");
    printf("%6s %6s  %s\n", "Faults", "Tests", "Location");
    printf("%6s %6s  %s\n", "------", "------", "--------");

    int count
        = faultline_for_each_hotspot(db, suite_name, limit, print_hotspot_row, NULL);
    if (count < 0) {
        printf("Error querying hotspots\n");
    } else if (count == 0) {
        printf("(no faults recorded)\n");
    }
}

/**
 * @brief Show test failures for a specific suite or all suites
 *
 * @param db Database connection
 * @param suite_name Suite name to filter by (NULL for all suites)
 * @param limit Maximum number of failures to show (0 = no limit)
 * @param show_all_history If true, show failures from all runs; if false (default), show
 * only most recent runs
 */
void faultline_show_test_failures(sqlite3 *db, char const *suite_name, int limit,
                                  bool show_all_history) {
    if (db == NULL) {
        printf("No database connection available\n");
        return;
    }

    // Build the base SQL query with optional filtering for recent runs only. The
    // suite filter is bound as ?1 by prepare_suite_report (NULL means all suites),
    // like the other report queries, so a suite name is never spliced into the SQL.
    char const *base_sql;
    if (show_all_history) {
        base_sql = "SELECT rtr.id, ts.suite_name, rts.test_name, rts.result_code, "
                   "       rts.exception_reason, rts.details, rf.source_file, "
                   "rf.source_line, rf.resource_address "
                   "FROM raw_test_summaries rts "
                   "JOIN raw_test_runs rtr ON rts.run_id = rtr.id "
                   "JOIN test_suites ts ON rtr.suite_id = ts.suite_id "
                   "LEFT JOIN raw_faults rf ON rts.id = rf.summary_id "
                   "WHERE (?1 IS NULL OR ts.suite_name = ?1) "
                   "AND rts.result_code > 1 " // Exclude FL_NOT_RUN (0) and FL_PASS (1)
                   "ORDER BY rtr.timestamp DESC";
    } else {
        // Default: show only failures from the most recent test run per suite
        base_sql = "SELECT rtr.id, ts.suite_name, rts.test_name, rts.result_code, "
                   "       rts.exception_reason, rts.details, rf.source_file, "
                   "rf.source_line, rf.resource_address "
                   "FROM raw_test_summaries rts "
                   "JOIN raw_test_runs rtr ON rts.run_id = rtr.id "
                   "JOIN test_suites ts ON rtr.suite_id = ts.suite_id "
                   "LEFT JOIN raw_faults rf ON rts.id = rf.summary_id "
                   "WHERE (?1 IS NULL OR ts.suite_name = ?1) "
                   "AND rts.result_code > 1 " // Exclude FL_NOT_RUN (0) and FL_PASS (1)
                   "AND rtr.timestamp >= ("
                   "    SELECT MAX(rtr2.timestamp) "
                   "    FROM raw_test_runs rtr2 "
                   "    WHERE rtr2.suite_id = rtr.suite_id"
                   ") "
                   "ORDER BY rtr.timestamp DESC";
    }

    sqlite3_stmt *stmt = prepare_suite_report(db, base_sql, suite_name, limit);
    if (stmt == NULL) {
        printf("Error preparing query\n");
        return;
    }

    // Column indices for test failures query
    enum {
        COL_RUN_ID = 0,
        COL_SUITE_NAME,
        COL_TEST_NAME,
        COL_RESULT_CODE,
        COL_REASON,
        COL_DETAILS,
        COL_SOURCE_FILE,
        COL_SOURCE_LINE,
        COL_RESOURCE_ADDR
    };

    printf("\n=== Test Failures ===\n");
    if (suite_name) {
        printf("Suite: %s\n", suite_name);
    }
    if (!show_all_history) {
        printf("Showing failures from most recent test runs only. Use --all-history to "
               "see all failures.\n");
    }
    printf("%-4s %-15s %-25s %-12s %-12s %-40s\n", "Run", "Suite", "Test", "Result",
           "Details", "Allocation Location");
    printf("%-4s %-15s %-25s %-12s %-12s %-40s\n", "----", "---------------",
           "-------------------------", "------------", "------------",
           "----------------------------------------");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int         run_id    = sqlite3_column_int(stmt, COL_RUN_ID);
        char const *suite     = (char const *)sqlite3_column_text(stmt, COL_SUITE_NAME);
        char const *test_name = (char const *)sqlite3_column_text(stmt, COL_TEST_NAME);
        int         result_code = sqlite3_column_int(stmt, COL_RESULT_CODE);
        char const *reason      = (char const *)sqlite3_column_text(stmt, COL_REASON);
        char const *details     = (char const *)sqlite3_column_text(stmt, COL_DETAILS);
        char const *source_file
            = (char const *)sqlite3_column_text(stmt, COL_SOURCE_FILE);
        int           source_line   = sqlite3_column_int(stmt, COL_SOURCE_LINE);
        sqlite3_int64 resource_addr = sqlite3_column_int64(stmt, COL_RESOURCE_ADDR);

        char const *result_str = faultline_result_code_to_string(result_code);

        // Format allocation location with resource address if available
        char location[41]; // 40 chars + null terminator
        if (source_file != NULL && source_line > 0) {
            // Extract just the filename from the full path
            char const *filename = strrchr(source_file, '\\');
            if (filename == NULL) {
                filename = strrchr(source_file, '/');
            }
            if (filename != NULL) {
                filename++;
            } else {
                filename = source_file;
            }

            // Include resource address for leaks and invalid frees when available
            if (resource_addr != 0 && result_code > FL_PASS) {
                // The stored address is 64-bit regardless of the width of the build
                // reading it, so format the value rather than round-tripping it
                // through a pointer that may be narrower.
                snprintf(location, sizeof location, "%s:%d @0x%llx", filename,
                         source_line, (unsigned long long)resource_addr);
            } else {
                snprintf(location, sizeof location, "%s:%d", filename, source_line);
            }
        } else if (reason != NULL && strlen(reason) > 0) {
            // Fall back to reason if no location available
            snprintf(location, sizeof location, "Reason: %.32s", reason);
        } else {
            strcpy_s(location, sizeof location, "No fault location recorded");
        }

        // printf("reason=%s, details=%s, source=%s:%d, %zd\n",reason, details,
        // source_file, source_line, resource_addr);
        printf("%-4d %-15.15s %-25.25s %-12s %-12s %-40.40s\n", run_id, suite, test_name,
               result_str,
               details != NULL && strlen(details) != 0 ? details : "no details",
               location);
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Show detailed information for a specific test run
 *
 * @param db Database connection
 * @param run_id Run ID to display details for
 */
void faultline_show_run_details(sqlite3 *db, int run_id) {
    if (db == NULL) {
        printf("No database connection available\n");
        return;
    }

    // First get run summary
    char const *run_sql
        = "SELECT ts.suite_name, rtr.timestamp, rtr.test_cases, rtr.tests_run, "
          "       rtr.tests_passed, rtr.total_elapsed_time, rtr.pass_rate, "
          "       rtr.total_fault_sites, rtr.discovery_failures, rtr.injection_failures "
          "FROM raw_test_runs rtr "
          "JOIN test_suites ts ON rtr.suite_id = ts.suite_id "
          "WHERE rtr.id = ?";

    sqlite3_stmt *stmt;
    int           rc = sqlite3_prepare_v2(db, run_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing run query: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Column indices for run summary query
    enum {
        RUN_COL_SUITE_NAME = 0,
        RUN_COL_TIMESTAMP,
        RUN_COL_TEST_CASES,
        RUN_COL_TESTS_RUN,
        RUN_COL_TESTS_PASSED,
        RUN_COL_ELAPSED_TIME,
        RUN_COL_PASS_RATE,
        RUN_COL_TOTAL_FAULT_SITES,
        RUN_COL_DISCOVERY_FAILURES,
        RUN_COL_INJECTION_FAILURES
    };

    sqlite3_bind_int(stmt, 1, run_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        char const *suite_name
            = (char const *)sqlite3_column_text(stmt, RUN_COL_SUITE_NAME);
        char const *timestamp
            = (char const *)sqlite3_column_text(stmt, RUN_COL_TIMESTAMP);
        int    test_cases         = sqlite3_column_int(stmt, RUN_COL_TEST_CASES);
        int    tests_run          = sqlite3_column_int(stmt, RUN_COL_TESTS_RUN);
        int    tests_passed       = sqlite3_column_int(stmt, RUN_COL_TESTS_PASSED);
        double elapsed_time       = sqlite3_column_double(stmt, RUN_COL_ELAPSED_TIME);
        double pass_rate          = sqlite3_column_double(stmt, RUN_COL_PASS_RATE);
        int    total_fault_sites  = sqlite3_column_int(stmt, RUN_COL_TOTAL_FAULT_SITES);
        int    discovery_failures = sqlite3_column_int(stmt, RUN_COL_DISCOVERY_FAILURES);
        int    injection_failures = sqlite3_column_int(stmt, RUN_COL_INJECTION_FAILURES);

        printf("\n=== Test Run Details (ID: %d) ===\n", run_id);
        printf("Suite: %s\n", suite_name);
        printf("Timestamp: %s\n", timestamp);
        printf("Test Cases: %d\n", test_cases);
        printf("Tests Run: %d\n", tests_run);
        printf("Tests Passed: %d (%.1f%%)\n", tests_passed, pass_rate * 100.0);
        printf("Total Runtime: %.2f seconds\n", elapsed_time);
        printf("Fault Sites Exercised: %d\n", total_fault_sites);
        printf("Discovery Failures: %d\n", discovery_failures);
        printf("Injection Failures: %d\n", injection_failures);

        sqlite3_finalize(stmt);

        // Now get individual test results with file/line information from faults
        char const *tests_sql
            = "SELECT rts.test_name, rts.result_code, rts.elapsed_seconds, "
              "rts.faults_exercised, "
              "       rts.failure_phase, rts.failure_type, rts.exception_reason, "
              "rts.details, "
              "       rf.source_file, rf.source_line "
              "FROM raw_test_summaries rts "
              "LEFT JOIN raw_faults rf ON rts.id = rf.summary_id AND rf.fault_index = 0 "
              "WHERE rts.run_id = ? "
              "ORDER BY rts.test_index";

        // Column indices for individual test results query
        enum {
            TEST_COL_TEST_NAME = 0,
            TEST_COL_RESULT_CODE,
            TEST_COL_ELAPSED,
            TEST_COL_FAULTS,
            TEST_COL_FAILURE_PHASE,
            TEST_COL_FAILURE_TYPE,
            TEST_COL_REASON,
            TEST_COL_DETAILS,
            TEST_COL_SOURCE_FILE,
            TEST_COL_SOURCE_LINE
        };

        rc = sqlite3_prepare_v2(db, tests_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, run_id);

            printf("\n--- Individual Test Results ---\n");
            printf("%-25s %-12s %-8s %-6s %-30s %-30s %-30s\n", "Test Name", "Result",
                   "Time(s)", "Faults", "Reason", "Details", "Location");
            printf("%-25s %-12s %-8s %-6s %-30s %-30s %-30s\n",
                   "-------------------------", "------------", "--------", "------",
                   "------------------------------", "------------------------------",
                   "------------------------------");

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                char const *test_name
                    = (char const *)sqlite3_column_text(stmt, TEST_COL_TEST_NAME);
                int         result_code = sqlite3_column_int(stmt, TEST_COL_RESULT_CODE);
                double      elapsed     = sqlite3_column_double(stmt, TEST_COL_ELAPSED);
                int         faults      = sqlite3_column_int(stmt, TEST_COL_FAULTS);
                char const *reason
                    = (char const *)sqlite3_column_text(stmt, TEST_COL_REASON);
                char const *details
                    = (char const *)sqlite3_column_text(stmt, TEST_COL_DETAILS);
                char const *source_file
                    = (char const *)sqlite3_column_text(stmt, TEST_COL_SOURCE_FILE);
                int source_line = sqlite3_column_int(stmt, TEST_COL_SOURCE_LINE);

                char const *result_str = faultline_result_code_to_string(result_code);

                // Format location string
                char location[31]; // 30 chars + null terminator
                if (source_file != NULL && source_line > 0) {
                    // Extract just the filename from the full path
                    char const *filename = strrchr(source_file, '\\');
                    if (filename == NULL) {
                        filename = strrchr(source_file, '/');
                    }
                    if (filename != NULL) {
                        filename++;
                    } else {
                        filename = source_file;
                    }

                    snprintf(location, sizeof location, "%s:%d", filename, source_line);
                } else if (result_code > FL_PASS) {
                    // Failure but no location info
                    strcpy_s(location, sizeof location, "no location recorded");
                } else {
                    // Passing test
                    strcpy_s(location, sizeof location, "");
                }

                printf("%-25.25s %-12s %8.3f %6d %-30.30s %-30.30s %-30.30s\n",
                       test_name, result_str, elapsed, faults, reason ? reason : "",
                       details ? details : "no details", location);
            }
            sqlite3_finalize(stmt);
        }
    } else {
        printf("Test run %d not found\n", run_id);
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Show summary statistics for a test suite
 *
 * @param db Database connection
 * @param suite_name Name of the test suite
 */
void faultline_show_suite_summary(sqlite3 *db, char const *suite_name) {
    if (db == NULL || suite_name == NULL) {
        printf("No database connection or suite name provided\n");
        return;
    }

    char const *sql = "SELECT COUNT(*) as total_runs, "
                      "       AVG(pass_rate) as avg_pass_rate, "
                      "       MIN(timestamp) as first_run, "
                      "       MAX(timestamp) as last_run, "
                      "       SUM(total_fault_sites) as total_faults, "
                      "       AVG(total_elapsed_time) as avg_runtime "
                      "FROM raw_test_runs rtr "
                      "JOIN test_suites ts ON rtr.suite_id = ts.suite_id "
                      "WHERE ts.suite_name = ?";

    sqlite3_stmt *stmt;
    int           rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing query: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Column indices for suite summary query
    enum {
        COL_TOTAL_RUNS = 0,
        COL_AVG_PASS_RATE,
        COL_FIRST_RUN,
        COL_LAST_RUN,
        COL_TOTAL_FAULTS,
        COL_AVG_RUNTIME
    };

    sqlite3_bind_text(stmt, 1, suite_name, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int         total_runs    = sqlite3_column_int(stmt, COL_TOTAL_RUNS);
        double      avg_pass_rate = sqlite3_column_double(stmt, COL_AVG_PASS_RATE);
        char const *first_run = (char const *)sqlite3_column_text(stmt, COL_FIRST_RUN);
        char const *last_run  = (char const *)sqlite3_column_text(stmt, COL_LAST_RUN);
        int         total_faults = sqlite3_column_int(stmt, COL_TOTAL_FAULTS);
        double      avg_runtime  = sqlite3_column_double(stmt, COL_AVG_RUNTIME);

        printf("\n=== Suite Summary: %s ===\n", suite_name);
        printf("Total Runs: %d\n", total_runs);
        if (total_runs > 0) {
            printf("Average Pass Rate: %.1f%%\n", avg_pass_rate * 100.0);
            printf("First Run: %s\n",
                   first_run ? first_run : "First Run state not recorded");
            printf("Last Run: %s\n",
                   last_run ? last_run : "Last Run state not recorded");
            printf("Total Fault Sites: %d\n", total_faults);
            printf("Average Runtime: %.2f seconds\n", avg_runtime);
        } else {
            printf("No runs found for this suite.\n");
        }
    } else {
        printf("Suite '%s' not found\n", suite_name);
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief List the registered test suites with their aggregate run health.
 *
 * One row per suite in test_suites, with counters aggregated from raw_test_runs
 * (the test_suites.total_runs/last_run_at columns are not maintained, so the
 * live runs are the source of truth). Suites that have never been run sort to the
 * bottom and show zero runs. Verbose mode adds the suite's distinct test-case
 * count (from test_case_evolution), its first run, average runtime, and total
 * failures.
 *
 * @param db Database connection
 * @param limit Maximum suites to display (0 = no limit)
 * @param verbose When true, show the wider statistics set
 */
void faultline_show_suites(sqlite3 *db, int limit, bool verbose) {
    if (db == NULL) {
        printf("No database connection available\n");
        return;
    }

    char const *base_sql;
    if (verbose) {
        base_sql = "SELECT ts.suite_name, "
                   "       COUNT(rtr.id) AS total_runs, "
                   "       MIN(rtr.timestamp) AS first_run, "
                   "       MAX(rtr.timestamp) AS last_run, "
                   "       AVG(rtr.pass_rate) AS avg_pass_rate, "
                   "       SUM(rtr.total_fault_sites) AS total_faults, "
                   "       AVG(rtr.total_elapsed_time) AS avg_runtime, "
                   "       SUM(rtr.tests_failed) AS total_failures, "
                   "       (SELECT COUNT(*) FROM test_case_evolution tce "
                   "          WHERE tce.suite_name = ts.suite_name) AS case_count "
                   "FROM test_suites ts "
                   "LEFT JOIN raw_test_runs rtr ON rtr.suite_id = ts.suite_id "
                   "GROUP BY ts.suite_id, ts.suite_name "
                   "ORDER BY last_run DESC";
    } else {
        base_sql = "SELECT ts.suite_name, "
                   "       COUNT(rtr.id) AS total_runs, "
                   "       MAX(rtr.timestamp) AS last_run, "
                   "       AVG(rtr.pass_rate) AS avg_pass_rate, "
                   "       SUM(rtr.total_fault_sites) AS total_faults "
                   "FROM test_suites ts "
                   "LEFT JOIN raw_test_runs rtr ON rtr.suite_id = ts.suite_id "
                   "GROUP BY ts.suite_id, ts.suite_name "
                   "ORDER BY last_run DESC";
    }

    char query[1024];
    if (limit > 0) {
        snprintf(query, sizeof query, "%s LIMIT %d;", base_sql, limit);
    } else {
        snprintf(query, sizeof query, "%s;", base_sql);
    }

    sqlite3_stmt *stmt;
    int           rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing query: %s\n", sqlite3_errmsg(db));
        return;
    }

    enum {
        COL_SUITE_NAME = 0,
        COL_TOTAL_RUNS,
        COL_MID,    // first_run (verbose) or last_run (non-verbose)
        COL_4,      // last_run (verbose) or avg_pass_rate (non-verbose)
        COL_5,      // avg_pass_rate (verbose) or total_faults (non-verbose)
        COL_FAULTS, // total_faults (verbose only)
        COL_RUNTIME,
        COL_FAILURES,
        COL_CASES
    };

    printf("\n=== Test Suites ===\n");
    if (verbose) {
        printf("%-30s %5s %5s %-19s %-19s %6s %7s %8s %6s\n", "Suite", "Runs", "Cases",
               "First Run", "Last Run", "Pass%", "Faults", "Time(s)", "Fails");
        printf("%-30s %5s %5s %-19s %-19s %6s %7s %8s %6s\n",
               "------------------------------", "-----", "-----", "-------------------",
               "-------------------", "------", "-------", "--------", "------");
    } else {
        printf("%-30s %5s %-19s %6s %7s\n", "Suite", "Runs", "Last Run", "Pass%",
               "Faults");
        printf("%-30s %5s %-19s %6s %7s\n", "------------------------------", "-----",
               "-------------------", "------", "-------");
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        char const *suite_name = (char const *)sqlite3_column_text(stmt, COL_SUITE_NAME);
        int         total_runs = sqlite3_column_int(stmt, COL_TOTAL_RUNS);

        if (verbose) {
            char const *first_run = (char const *)sqlite3_column_text(stmt, COL_MID);
            char const *last_run  = (char const *)sqlite3_column_text(stmt, COL_4);
            double      pass_rate = sqlite3_column_double(stmt, COL_5);
            int         faults    = sqlite3_column_int(stmt, COL_FAULTS);
            double      runtime   = sqlite3_column_double(stmt, COL_RUNTIME);
            int         failures  = sqlite3_column_int(stmt, COL_FAILURES);
            int         cases     = sqlite3_column_int(stmt, COL_CASES);

            printf("%-30.30s %5d %5d %-19.19s %-19.19s %5.1f%% %7d %8.3f %6d\n",
                   suite_name, total_runs, cases, first_run ? first_run : "-",
                   last_run ? last_run : "-", pass_rate * 100.0, faults, runtime,
                   failures);
        } else {
            char const *last_run  = (char const *)sqlite3_column_text(stmt, COL_MID);
            double      pass_rate = sqlite3_column_double(stmt, COL_4);
            int         faults    = sqlite3_column_int(stmt, COL_5);

            printf("%-30.30s %5d %-19.19s %5.1f%% %7d\n", suite_name, total_runs,
                   last_run ? last_run : "-", pass_rate * 100.0, faults);
        }
        count++;
    }

    if (count == 0) {
        printf("(no suites recorded)\n");
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief List the test-case catalog from recorded run history.
 *
 * Reads test_case_evolution, which holds one row per (suite, test) seen across
 * all runs, optionally filtered to a single suite. Each row shows how often the
 * test ran, how often it failed, and its latest fault-site count. Verbose mode
 * adds the baseline-vs-latest comparison (fault sites and runtime) and the
 * baseline date, so coverage or runtime drift is visible at a glance.
 *
 * test_case_evolution is populated best-effort and was rebuilt on the v1->v2
 * schema migration, so a database whose suites predate that may list few or no
 * cases until the suites are run again.
 *
 * @param db Database connection
 * @param suite_name Suite to filter by, or NULL for all suites
 * @param limit Maximum cases to display (0 = no limit)
 * @param verbose When true, show the baseline comparison columns
 */
void faultline_show_cases(sqlite3 *db, char const *suite_name, int limit, bool verbose) {
    if (db == NULL) {
        printf("No database connection available\n");
        return;
    }

    char const *base_sql;
    if (verbose) {
        base_sql = "SELECT suite_name, test_name, total_appearances, total_failures, "
                   "       baseline_fault_sites, last_fault_sites, "
                   "       baseline_execution_time, last_execution_time, baseline_date "
                   "FROM test_case_evolution "
                   "WHERE (?1 IS NULL OR suite_name = ?1) "
                   "ORDER BY suite_name, test_name";
    } else {
        base_sql = "SELECT suite_name, test_name, total_appearances, total_failures, "
                   "       last_fault_sites "
                   "FROM test_case_evolution "
                   "WHERE (?1 IS NULL OR suite_name = ?1) "
                   "ORDER BY suite_name, test_name";
    }

    sqlite3_stmt *stmt = prepare_suite_report(db, base_sql, suite_name, limit);
    if (stmt == NULL) {
        printf("Error preparing query\n");
        return;
    }

    enum {
        COL_SUITE_NAME = 0,
        COL_TEST_NAME,
        COL_APPEARANCES,
        COL_FAILURES,
        COL_BASE_FAULTS, // verbose
        COL_LAST_FAULTS, // verbose; non-verbose reuses index 4
        COL_BASE_TIME,
        COL_LAST_TIME,
        COL_BASE_DATE
    };

    printf("\n=== Test Cases ===\n");
    if (suite_name != NULL) {
        printf("Suite: %s\n", suite_name);
    }
    if (verbose) {
        printf("%-25s %-25s %5s %5s %5s %5s %8s %8s %-19s\n", "Suite", "Test", "Runs",
               "Fails", "Base", "Last", "BTime", "LTime", "Baseline");
        printf("%-25s %-25s %5s %5s %5s %5s %8s %8s %-19s\n",
               "-------------------------", "-------------------------", "-----",
               "-----", "-----", "-----", "--------", "--------", "-------------------");
    } else {
        printf("%-25s %-30s %5s %5s %7s\n", "Suite", "Test", "Runs", "Fails", "Faults");
        printf("%-25s %-30s %5s %5s %7s\n", "-------------------------",
               "------------------------------", "-----", "-----", "-------");
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        char const *suite = (char const *)sqlite3_column_text(stmt, COL_SUITE_NAME);
        char const *test  = (char const *)sqlite3_column_text(stmt, COL_TEST_NAME);
        int         runs  = sqlite3_column_int(stmt, COL_APPEARANCES);
        int         fails = sqlite3_column_int(stmt, COL_FAILURES);

        if (verbose) {
            int         base_faults = sqlite3_column_int(stmt, COL_BASE_FAULTS);
            int         last_faults = sqlite3_column_int(stmt, COL_LAST_FAULTS);
            double      base_time   = sqlite3_column_double(stmt, COL_BASE_TIME);
            double      last_time   = sqlite3_column_double(stmt, COL_LAST_TIME);
            char const *base_date
                = (char const *)sqlite3_column_text(stmt, COL_BASE_DATE);

            printf("%-25.25s %-25.25s %5d %5d %5d %5d %8.3f %8.3f %-19.19s\n", suite,
                   test, runs, fails, base_faults, last_faults, base_time, last_time,
                   base_date ? base_date : "-");
        } else {
            int last_faults = sqlite3_column_int(stmt, COL_BASE_FAULTS); // index 4
            printf("%-25.25s %-30.30s %5d %5d %7d\n", suite, test, runs, fails,
                   last_faults);
        }
        count++;
    }

    if (count == 0) {
        printf("(no cases recorded)\n");
    }

    sqlite3_finalize(stmt);
}

void faultline_sqlite_migrate_schema(char const *db_path, int current_version) {
    sqlite3 *db;

    // Only open existing database for reading
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);

    if (rc != SQLITE_OK) {
        // Clear: database doesn't exist or not readable
        char details[256];
        strcpy_s(details, sizeof details, db ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
        sqlite3_close_v2(db);
        FL_THROW_DETAILS(faultline_db_not_found, "sqlite3: %s", details);
    }

    // TBD ...
    // Extract current version from the database and update from its version to the
    // current version
    FL_UNUSED(current_version);
    sqlite3_close_v2(db);
}

void faultline_export_sqlite(FLContext *fctx, char const *db_path) {
    sqlite3 *db;

    // temporary
    FL_UNUSED(fctx);

    // Create new database or open existing for writing
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             NULL);

    if (rc != SQLITE_OK) {
        // Clear error handling - know exactly what failed
        char details[256];
        strcpy_s(details, sizeof details, db ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
        sqlite3_close_v2(db);
        FL_THROW_DETAILS(faultline_db_create_failed, "sqlite3: %s", details);
    }
    // ...
    sqlite3_close_v2(db);
}

void faultline_import_sqlite(FLContext *fctx, char const *db_path, int run_id) {
    sqlite3 *db;

    // temporary
    FL_UNUSED(fctx);
    FL_UNUSED(run_id);

    // Only open existing database for reading
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);

    if (rc != SQLITE_OK) {
        // Clear: database doesn't exist or not readable
        char details[256];
        strcpy_s(details, sizeof details, db ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
        sqlite3_close_v2(db);
        FL_THROW_DETAILS(faultline_db_not_found, "sqlite3: %s", details);
    }
    // ...
    sqlite3_close_v2(db);
}
