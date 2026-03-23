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
#include <string.h>                         // for strcpy_s, strrchr, strlen, strcat_s
#include <time.h>                           // for time_t
#include <faultline/fl_abbreviated_types.h> // for u32, i64
#include <faultline/fl_exception_service.h> // for FL_REASON
#include <faultline/fl_macros.h>            // for FL_UNUSED
#include <faultline/fl_test.h>              // for FLTestSuite
#include "flp_time.h"                       // for fl_gmtime

// Schema version management
#define FL_SCHEMA_VERSION 1

FLExceptionReason faultline_db_create_failed = "failed to create database";
FLExceptionReason faultline_db_not_found     = "database not found";

static char const *faultline_db = "Faultline DB Initialization";

// Table creation statements
static char const *schema_tables[] = {
    // Test Suite Registry - tracks different test suites being tested over time
    "CREATE TABLE IF NOT EXISTS test_suites ("
    "  suite_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  suite_name TEXT NOT NULL UNIQUE,"
    "  description TEXT,"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "  last_run_at DATETIME,"
    "  total_runs INTEGER DEFAULT 0"
    ");",

    // Test runs table - tracks each execution
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

    // Raw test summaries - direct FLTestSummary mapping
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

    // Raw fault data - direct Fault structure mapping
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

    // Schema version tracking
    "CREATE TABLE IF NOT EXISTS schema_info ("
    "  version INTEGER PRIMARY KEY,"
    "  applied_date TEXT NOT NULL"
    ");",
    NULL // Terminator
};

// Query optimization tables - SHOULD succeed on creation
static char const *schema_analysis_tables[] = {
    // ============================================================================
    // Layer 2: Query-Optimized Tables (Common Access Patterns)
    // ============================================================================
    //
    // Failure-focused table for debugging workflows
    "CREATE TABLE IF NOT EXISTS test_failures ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    run_id INTEGER NOT NULL,"
    "    suite_name TEXT NOT NULL,"
    "    test_name TEXT NOT NULL,"
    "    test_index INTEGER NOT NULL,"
    ""
    "    failure_phase TEXT NOT NULL,"
    "    failure_type TEXT NOT NULL,"
    ""
    "    result_code INTEGER NOT NULL,"
    "    exception_reason TEXT,"
    "    source_file TEXT,"
    "    source_line INTEGER,"
    ""
    "    fault_index INTEGER,"
    "    resource_address INTEGER,"
    "    elapsed_seconds REAL,"
    "    timestamp DATETIME,"
    ""
    "    FOREIGN KEY (run_id) REFERENCES raw_test_runs(id) ON DELETE CASCADE"
    ");",

    // Test case evolution tracking
    "CREATE TABLE IF NOT EXISTS test_case_evolution ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    suite_name TEXT NOT NULL,"
    "    test_name TEXT NOT NULL,"
    "    first_seen_run_id INTEGER NOT NULL,"
    "    last_seen_run_id INTEGER NOT NULL,"
    "    total_appearances INTEGER DEFAULT 1,"
    "    total_failures INTEGER DEFAULT 0,"
    "    avg_execution_time REAL,"
    "    avg_fault_sites REAL,"
    ""
    "    FOREIGN KEY (first_seen_run_id) REFERENCES raw_test_runs(id),"
    "    FOREIGN KEY (last_seen_run_id) REFERENCES raw_test_runs(id)"
    ");",

    // Fault hotspots - frequently failing source locations
    "CREATE TABLE IF NOT EXISTS fault_hotspots ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    source_file TEXT NOT NULL,"
    "    source_line INTEGER NOT NULL,"
    "    failure_count INTEGER NOT NULL,"
    "    first_failure_run_id INTEGER NOT NULL,"
    "    last_failure_run_id INTEGER NOT NULL,"
    "    common_failure_types TEXT,"
    ""
    "   FOREIGN KEY (first_failure_run_id) REFERENCES raw_test_runs(id),"
    "   FOREIGN KEY (last_failure_run_id) REFERENCES raw_test_runs(id)"
    ");",

    // ============================================================================
    // Layer 3: Aggregated Metrics (Trend Analysis)
    // ============================================================================

    // Daily test suite metrics rollup
    "CREATE TABLE IF NOT EXISTS daily_suite_metrics ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    suite_name TEXT NOT NULL,"
    "    date DATE NOT NULL,"
    ""
    "    total_runs INTEGER NOT NULL,"
    "    avg_pass_rate REAL,"
    "    avg_execution_time REAL,"
    "    total_test_cases INTEGER,"
    "    avg_fault_sites REAL,"
    ""
    "   total_failures INTEGER,"
    "   discovery_failure_rate REAL,"
    "   injection_failure_rate REAL,"
    "   leak_failure_count INTEGER,"
    ""
    "    fastest_run_time REAL,"
    "    slowest_run_time REAL,"
    ""
    "    UNIQUE(suite_name, date)"
    ");",

    // Test case performance baselines
    "CREATE TABLE IF NOT EXISTS test_case_baselines ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    suite_name TEXT NOT NULL,"
    "    test_name TEXT NOT NULL,"
    ""
    "    baseline_execution_time REAL,"
    "    baseline_fault_sites INTEGER,"
    "    baseline_established_date DATE,"
    "    baseline_run_count INTEGER,"
    ""
    "    last_execution_time REAL,"
    "    execution_time_deviation_pct REAL,"
    "    last_fault_sites INTEGER,"
    "    fault_sites_deviation_pct REAL,"
    ""
    "    UNIQUE(suite_name, test_name)"
    ");",

    // Suite evolution summary
    "CREATE TABLE IF NOT EXISTS suite_evolution_summary ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    suite_name TEXT NOT NULL,"
    "    month TEXT NOT NULL,"
    ""
    "    test_cases_added INTEGER DEFAULT 0,"
    "    test_cases_removed INTEGER DEFAULT 0,"
    "    test_cases_renamed INTEGER DEFAULT 0,"
    ""
    "    avg_pass_rate REAL,"
    "    pass_rate_trend TEXT,"
    "    total_runs INTEGER,"
    ""
    "    UNIQUE(suite_name, month)"
    ");",
    NULL // Terminator
};

// Performance indexes - CAN fail silently on creation
static char const *schema_indexes[] = {
    // ============================================================================
    // Layer 4: Indexes for Performance
    // ============================================================================

    // Performance indexes
    "CREATE INDEX IF NOT EXISTS idx_raw_test_runs_suite_timestamp ON "
    "raw_test_runs(suite_id, timestamp);",
    "CREATE INDEX IF NOT EXISTS idx_raw_test_summaries_run_index ON "
    "raw_test_summaries(run_id, test_index);",
    "CREATE INDEX IF NOT EXISTS idx_raw_faults_summary_index ON raw_faults(summary_id, "
    "fault_index);",
    "CREATE INDEX IF NOT EXISTS idx_test_failures_suite_timestamp ON "
    "test_failures(suite_name, timestamp);",
    "CREATE INDEX IF NOT EXISTS idx_test_failures_type_phase ON "
    "test_failures(failure_type, failure_phase);",
    "CREATE INDEX IF NOT EXISTS idx_fault_hotspots_location ON "
    "fault_hotspots(source_file, source_line);",
    "CREATE INDEX IF NOT EXISTS idx_daily_metrics_suite_date ON "
    "daily_suite_metrics(suite_name, date);",
    NULL // Terminator
};

// Convenience views - CAN fail silently on creation
static char const *schema_views[] = {
    // ============================================================================
    // Layer 4: Convenience Views
    // ============================================================================

    // Latest run for each suite
    "CREATE VIEW IF NOT EXISTS latest_runs AS "
    "SELECT "
    "    rtr.*,"
    "    COUNT(rts.id) as executed_test_cases,"
    "    SUM(CASE WHEN rts.result_code = 0 THEN 1 ELSE 0 END) as successful_cases "
    "FROM raw_test_runs rtr "
    "LEFT JOIN raw_test_summaries rts ON rtr.id = rts.run_id "
    "WHERE rtr.timestamp = ("
    "    SELECT MAX(timestamp) "
    "    FROM raw_test_runs rtr2 "
    "    WHERE rtr2.suite_name = rtr.suite_name"
    ")"
    "GROUP BY rtr.id;",

    // Failure summary by test case (for debugging)
    "CREATE VIEW IF NOT EXISTS test_case_failure_summary AS "
    "SELECT "
    "    tf.suite_name,"
    "    tf.test_name,"
    "    COUNT(*) as total_failures,"
    "    COUNT(DISTINCT tf.run_id) as failed_in_runs,"
    "    GROUP_CONCAT(DISTINCT tf.failure_type) as failure_types,"
    "    GROUP_CONCAT(DISTINCT tf.failure_phase) as failure_phases,"
    "    AVG(tf.elapsed_seconds) as avg_failure_time,"
    "    MAX(tf.timestamp) as last_failure "
    "FROM test_failures tf "
    "GROUP BY tf.suite_name, tf.test_name "
    "ORDER BY total_failures DESC;",

    // Performance regression detection
    "CREATE VIEW IF NOT EXISTS performance_regressions AS "
    "SELECT "
    "    tcb.suite_name,"
    "    tcb.test_name,"
    "    tcb.baseline_execution_time,"
    "    tcb.last_execution_time,"
    "    tcb.execution_time_deviation_pct,"
    "    tcb.baseline_fault_sites,"
    "    tcb.last_fault_sites,"
    "    tcb.fault_sites_deviation_pct,"
    "    CASE "
    "        WHEN tcb.execution_time_deviation_pct > 20 THEN 'SLOW_REGRESSION' "
    "        WHEN tcb.fault_sites_deviation_pct < -10 THEN 'COVERAGE_REGRESSION' "
    "        ELSE 'STABLE' "
    "    END as regression_type "
    "FROM test_case_baselines tcb "
    "WHERE tcb.execution_time_deviation_pct > 20 "
    "    OR tcb.fault_sites_deviation_pct < -10;",

    // Trend analysis view
    "CREATE VIEW IF NOT EXISTS suite_trends AS "
    "SELECT "
    "    dsm.suite_name,"
    "    dsm.date,"
    "    dsm.avg_pass_rate,"
    "    LAG(dsm.avg_pass_rate) OVER (PARTITION BY dsm.suite_name ORDER BY dsm.date) as "
    "prev_pass_rate,"
    "    dsm.avg_execution_time,"
    "    LAG(dsm.avg_execution_time) OVER (PARTITION BY dsm.suite_name ORDER BY "
    "dsm.date) as prev_execution_time,"
    "    dsm.total_failures,"
    "    LAG(dsm.total_failures) OVER (PARTITION BY dsm.suite_name ORDER BY dsm.date) "
    "as prev_failures "
    "FROM daily_suite_metrics dsm "
    "ORDER BY dsm.suite_name, dsm.date;",
    NULL // Terminator
};

static char const *schema_triggers[] = {
    // ============================================================================
    // Triggers for Maintaining Derived Data
    // ============================================================================

    // Update test_case_evolution when new test summaries are inserted
    "CREATE TRIGGER IF NOT EXISTS update_test_evolution "
    "AFTER INSERT ON raw_test_summaries "
    "BEGIN "
    "    INSERT OR REPLACE INTO test_case_evolution ("
    "        suite_name, test_name, first_seen_run_id, last_seen_run_id,"
    "        total_appearances, total_failures, avg_execution_time, avg_fault_sites "
    "    ) "
    "    SELECT "
    "        rtr.suite_name,"
    "        NEW.test_name,"
    "        COALESCE((SELECT first_seen_run_id FROM test_case_evolution "
    "                WHERE suite_name = rtr.suite_name AND test_name = NEW.test_name), "
    "NEW.run_id),"
    "        NEW.run_id,"
    "        COALESCE((SELECT total_appearances FROM test_case_evolution "
    "                WHERE suite_name = rtr.suite_name AND test_name = NEW.test_name), "
    "0) + 1,"
    "        COALESCE((SELECT total_failures FROM test_case_evolution "
    "                WHERE suite_name = rtr.suite_name AND test_name = NEW.test_name), "
    "0) + "
    "                CASE WHEN NEW.result_code != 0 THEN 1 ELSE 0 END,"
    "        (SELECT AVG(elapsed_seconds) FROM raw_test_summaries rts2 "
    "        JOIN raw_test_runs rtr2 ON rts2.run_id = rtr2.id "
    "        WHERE rtr2.suite_name = rtr.suite_name AND rts2.test_name = NEW.test_name),"
    "        (SELECT AVG(faults_exercised) FROM raw_test_summaries rts2 "
    "        JOIN raw_test_runs rtr2 ON rts2.run_id = rtr2.id "
    "        WHERE rtr2.suite_name = rtr.suite_name AND rts2.test_name = NEW.test_name) "
    "    FROM raw_test_runs rtr WHERE rtr.id = NEW.run_id; "
    "END;",
    NULL // Terminator
};

typedef struct {
    int         from_version;
    int         to_version;
    char const *sql;
} SchemaMigration;

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
                 sqlite3_errmsg(db));
        sqlite3_close_v2(db);
        LOG_ERROR(faultline_db, "%s", details);
        FL_THROW_DETAILS(faultline_db_create_failed, "sqlite3: %s", details);
        return NULL;
    }
    LOG_DEBUG(faultline_db, "Database opened successfully");

    // Initialize schema on the opened connection
    FL_TRY {
        // Create all tables
        LOG_DEBUG(faultline_db, "Creating core tables...");
        for (int i = 0; schema_tables[i] != NULL; i++) {
            LOG_DEBUG(faultline_db, "Creating table %d", i);
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

        // Create indexes (can fail silently - performance optimization)
        LOG_DEBUG(faultline_db, "Creating indexes...");
        for (int i = 0; schema_indexes[i] != NULL; i++) {
            sqlite3_exec(db, schema_indexes[i], NULL, NULL, NULL);
        }
        LOG_DEBUG(faultline_db, "Indexes created");

        // Create views (can fail silently - convenience feature)
        LOG_DEBUG(faultline_db, "Creating views...");
        for (int i = 0; schema_views[i] != NULL; i++) {
            sqlite3_exec(db, schema_views[i], NULL, NULL, NULL);
        }
        LOG_DEBUG(faultline_db, "Views created");

        LOG_VERBOSE(faultline_db, "Database initialized successfully: %s", db_path);
    }
    FL_CATCH_ALL {
        LOG_ERROR(faultline_db, "Exception during database initialization");
        sqlite3_close_v2(db);
        FL_RETHROW;
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
    sqlite3 *db;
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             NULL);
    if (rc != SQLITE_OK) {
        char details[256];
        strcpy_s(details, sizeof details, sqlite3_errmsg(db));
        sqlite3_close_v2(db);
        FL_THROW_DETAILS(faultline_db_create_failed, "sqlite3: %s: %s", details,
                         db_path);
    }

    // Core tables - MUST succeed
    for (int i = 0; schema_tables[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_tables[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            char details[256];
            strcpy_s(details, sizeof details, sqlite3_errmsg(db));
            sqlite3_close_v2(db);
            FL_THROW_DETAILS(faultline_db_create_failed,
                             "Core table creation failed: %s: %s", details, db_path);
        }
    }

    // Analysis tables - SHOULD succeed but not fatal
    for (int i = 0; schema_analysis_tables[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_analysis_tables[i], NULL, NULL, NULL);
        // Log but don't fail - these are for analytics
        if (rc != SQLITE_OK) {
            LOG_WARN(faultline_db, "Analysis table creation failed: %s",
                     sqlite3_errmsg(db));
        }
    }

    // Indexes - CAN fail silently
    for (int i = 0; schema_indexes[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_indexes[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            LOG_WARN(faultline_db, "Index creation failed: %s", sqlite3_errmsg(db));
        }
    }

    // Views - CAN fail silently
    for (int i = 0; schema_views[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_views[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            LOG_WARN(faultline_db, "View creation failed: %s", sqlite3_errmsg(db));
        }
    }

    // Triggers - CAN fail silently
    for (int i = 0; schema_triggers[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_triggers[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            LOG_WARN(faultline_db, "Trigger creation failed: %s", sqlite3_errmsg(db));
        }
    }

    // Record schema version
    char version_sql[256];
    snprintf(version_sql, sizeof(version_sql),
             "INSERT OR REPLACE INTO schema_info (version, applied_date) "
             "VALUES (%d, datetime('now'));",
             FL_SCHEMA_VERSION);
    rc = sqlite3_exec(db, version_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        LOG_WARN(faultline_db, "Schema Version table creation failed: %s",
                 sqlite3_errmsg(db));
    }

    sqlite3_close_v2(db);
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

    int        suite_id = -1;
    int        run_id   = -1;
    struct tm  tm_result;
    struct tm *tm_info = fl_gmtime(&start_time, &tm_result);
    char       timestamp[32];
    if (tm_info) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
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
                  "    suite_id, test_cases, tests_run, tests_passed, "
                  "tests_passed_with_leaks, "
                  "    tests_failed, setups_failed, cleanups_failed, total_fault_sites, "
                  "    total_elapsed_time"
                  ") VALUES (?, ?, 0, 0, 0, 0, 0, 0, 0, 0.0);";

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

            if (summary->code == FL_LEAK)
                leak_failures++;
            if (summary->code == FL_INVALID_FREE || summary->code == FL_DOUBLE_FREE)
                invalid_free_failures++;
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
                             0); // calculated separately
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
                                  "Fault %zu: file=%s, line=%d, reason=%s, resource=%p",
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
        int         test_cases = sqlite3_column_int(stmt, COL_TEST_CASES);
        int         tests_run  = sqlite3_column_int(stmt, COL_TESTS_RUN);
        int         tests_passed = sqlite3_column_int(stmt, COL_TESTS_PASSED);
        double      elapsed_time = sqlite3_column_double(stmt, COL_ELAPSED_TIME);
        double      pass_rate    = sqlite3_column_double(stmt, COL_PASS_RATE);
        int         fault_sites  = sqlite3_column_int(stmt, COL_FAULT_SITES);

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

    // Build the base SQL query with optional filtering for recent runs only
    char const *base_sql;
    if (show_all_history) {
        base_sql = "SELECT rtr.id, ts.suite_name, rts.test_name, rts.result_code, "
                   "       rts.exception_reason, rts.details, rf.source_file, "
                   "rf.source_line, rf.resource_address "
                   "FROM raw_test_summaries rts "
                   "JOIN raw_test_runs rtr ON rts.run_id = rtr.id "
                   "JOIN test_suites ts ON rtr.suite_id = ts.suite_id "
                   "LEFT JOIN raw_faults rf ON rts.id = rf.summary_id "
                   "WHERE rts.result_code > 1"; // Exclude FL_NOT_RUN (0) and
                                                // FL_PASS (1)
    } else {
        // Default: show only failures from the most recent test run per suite
        base_sql = "SELECT rtr.id, ts.suite_name, rts.test_name, rts.result_code, "
                   "       rts.exception_reason, rts.details, rf.source_file, "
                   "rf.source_line, rf.resource_address "
                   "FROM raw_test_summaries rts "
                   "JOIN raw_test_runs rtr ON rts.run_id = rtr.id "
                   "JOIN test_suites ts ON rtr.suite_id = ts.suite_id "
                   "LEFT JOIN raw_faults rf ON rts.id = rf.summary_id "
                   "WHERE rts.result_code > 1 "
                   "AND rtr.timestamp >= ("
                   "    SELECT MAX(rtr2.timestamp) "
                   "    FROM raw_test_runs rtr2 "
                   "    WHERE rtr2.suite_id = rtr.suite_id"
                   ")";
    }

    char query[1024]; // Increased buffer size for longer SQL queries with subqueries
    if (suite_name != NULL) {
        snprintf(query, sizeof query, "%s AND ts.suite_name = '%s'", base_sql,
                 suite_name);
    } else {
        strcpy_s(query, sizeof query, base_sql);
    }

    if (limit > 0) {
        char limit_clause[64];
        snprintf(limit_clause, sizeof limit_clause,
                 " ORDER BY rtr.timestamp DESC LIMIT %d", limit);
        strcat_s(query, sizeof query, limit_clause);
    } else {
        strcat_s(query, sizeof query, " ORDER BY rtr.timestamp DESC");
    }

    sqlite3_stmt *stmt;
    int           rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing query: %s\n", sqlite3_errmsg(db));
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
            if (filename == NULL)
                filename = strrchr(source_file, '/');
            if (filename != NULL)
                filename++;
            else
                filename = source_file;

            // Include resource address for leaks and invalid frees when available
            if (resource_addr != 0 && result_code > FL_PASS) {
                snprintf(location, sizeof location, "%s:%d @%p", filename, source_line,
                         (void *)resource_addr);
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
                    if (filename == NULL)
                        filename = strrchr(source_file, '/');
                    if (filename != NULL)
                        filename++;
                    else
                        filename = source_file;

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

void faultline_sqlite_migrate_schema(char const *db_path, int current_version) {
    sqlite3 *db;

    // Only open existing database for reading
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);

    if (rc != SQLITE_OK) {
        // Clear: database doesn't exist or not readable
        char details[256];
        strcpy_s(details, sizeof details, sqlite3_errmsg(db));
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
        strcpy_s(details, sizeof details, sqlite3_errmsg(db));
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
        strcpy_s(details, sizeof details, sqlite3_errmsg(db));
        sqlite3_close_v2(db);
        FL_THROW_DETAILS(faultline_db_not_found, "sqlite3: %s", details);
    }
    // ...
    sqlite3_close_v2(db);
}
