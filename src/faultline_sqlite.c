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
#include <faultline/fl_exception_types.h>   // FLExceptionReason
#include <faultline/fl_log.h>               // LOG_DEBUG, LOG_ERROR, LOG_WARN
#include <faultline/fl_try.h>               // FL_THROW_DETAILS, FL_CATCH_ALL
#include <sqlite/sqlite3.h>                 // sqlite3_exec, sqlite3_prepare_v2
#include <stdbool.h>                        // bool
#include <stdio.h>                          // snprintf
#include <faultline/fl_macros.h>            // FL_ARRAY_COUNT, FL_STATIC_ASSERT

#if defined(__cplusplus)
extern "C" {
#endif

FLExceptionReason faultline_db_create_failed = "failed to create database";
FLExceptionReason faultline_db_version_too_new
    = "database was created by a newer version of faultline";
FLExceptionReason faultline_db_legacy_schema
    = "database uses the schema that preceded version 1";

#if defined(__cplusplus)
}
#endif

static char const *faultline_db = "Faultline DB Initialization";

// ---- SQLite statement primitives ------------------------------------------
//
// Each takes a `what` label naming the operation for the failure log, e.g.
// "schema version query". sqlite3_prepare_v2 sets its out-parameter to NULL on error
// and sqlite3_finalize(NULL) is a no-op, so callers need no extra guarding
// around either.

/**
 * @brief Prepare a statement, logging on failure.
 *
 * @param db Database connection
 * @param sql Statement text
 * @param what Short description of the operation, used in the failure log
 * @return Prepared statement, or NULL on a prepare error.
 */
static sqlite3_stmt *db_prepare(sqlite3 *db, char const *sql, char const *what) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        LOG_WARN(faultline_db, "%s prepare failed: %s", what, sqlite3_errmsg(db));
    }
    return stmt;
}

/**
 * @brief Run a parameterless statement that returns no rows.
 *
 * @return true when the statement succeeded.
 */
static bool db_exec(sqlite3 *db, char const *sql, char const *what) {
    bool ok = (sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK);
    if (!ok) {
        LOG_WARN(faultline_db, "%s failed: %s", what, sqlite3_errmsg(db));
    }
    return ok;
}

/**
 * @brief Read column 0 of the first row of a parameterless query.
 *
 * @param fallback Value returned when the query fails or yields no row
 */
static int db_query_int(sqlite3 *db, char const *sql, int fallback, char const *what) {
    int           value = fallback;
    sqlite3_stmt *stmt  = db_prepare(db, sql, what);
    if (stmt != NULL) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            value = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return value;
}

/*
 * Schema v1: one row per suite, test case, run, case result, and fault. Totals are not
 * stored; the run_totals view computes them from results.
 *
 *   suites  1---n  cases
 *   suites  1---n  runs
 *   runs    1---n  results  n---1  cases
 *   results 1---n  faults
 *   cases.baseline_run_id ---> runs
 *
 * result_code, failure_type and failure_phase hold FLResultCode, FLFailureType and
 * FLTestPhase values. Each CHECK range runs from an enum's first value to its last.
 */
static char const *const schema_v1[] = {
    "CREATE TABLE suites ("
    "    id         INTEGER PRIMARY KEY,"
    "    name       TEXT NOT NULL UNIQUE,"
    "    created_at TEXT NOT NULL DEFAULT (strftime('" FL_DB_TIME_FORMAT "', 'now'))"
    ");",

    // AUTOINCREMENT never reuses an id, even after runs are deleted, so the latest run of
    // a suite is the one with the largest id.
    "CREATE TABLE runs ("
    "    id           INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    suite_id     INTEGER NOT NULL REFERENCES suites (id),"
    "    started_at   TEXT NOT NULL,"
    "    completed_at TEXT NOT NULL,"
    "    case_count   INTEGER NOT NULL CHECK (case_count >= 0)"
    ");",

    // A case is identified by its name within its suite. The baseline is the run that
    // regression reports compare the case against.
    "CREATE TABLE cases ("
    "    id              INTEGER PRIMARY KEY,"
    "    suite_id        INTEGER NOT NULL REFERENCES suites (id),"
    "    name            TEXT NOT NULL,"
    "    baseline_run_id INTEGER REFERENCES runs (id) ON DELETE SET NULL,"
    "    baseline_set_at TEXT,"
    "    UNIQUE (suite_id, name)"
    ");",

    // One result per case per run: result_code 0..FL_BAD_STATE, failure_type
    // 0..FL_INVALID_FREE_FAILURE, failure_phase 0..FL_INJECTION_PHASE. A result records a
    // phase exactly when failure_type is not FL_FAILURE_NONE.
    "CREATE TABLE results ("
    "    id                 INTEGER PRIMARY KEY,"
    "    run_id             INTEGER NOT NULL REFERENCES runs (id) ON DELETE CASCADE,"
    "    case_id            INTEGER NOT NULL REFERENCES cases (id),"
    "    case_index         INTEGER NOT NULL CHECK (case_index >= 0),"
    "    result_code        INTEGER NOT NULL CHECK (result_code BETWEEN 0 AND 6),"
    "    failure_type       INTEGER NOT NULL CHECK (failure_type BETWEEN 0 AND 5),"
    "    failure_phase      INTEGER CHECK (failure_phase BETWEEN 0 AND 1),"
    "    reason             TEXT,"
    "    details            TEXT,"
    "    elapsed_seconds    REAL NOT NULL,"
    "    discovery_seconds  REAL NOT NULL,"
    "    injection_seconds  REAL NOT NULL,"
    "    fault_sites        INTEGER NOT NULL CHECK (fault_sites >= 0),"
    "    discovery_failures INTEGER NOT NULL CHECK (discovery_failures >= 0),"
    "    injection_failures INTEGER NOT NULL CHECK (injection_failures >= 0),"
    "    CHECK ((failure_type = 0) = (failure_phase IS NULL)),"
    "    UNIQUE (run_id, case_index),"
    "    UNIQUE (run_id, case_id)"
    ");",

    // position is the fault's place in its result's list. fault_index is the injector
    // threshold that produced the fault; one threshold can produce several faults, such
    // as an invalid free and a leak, so it is not unique within a result.
    "CREATE TABLE faults ("
    "    id               INTEGER PRIMARY KEY,"
    "    result_id        INTEGER NOT NULL REFERENCES results (id) ON DELETE CASCADE,"
    "    position         INTEGER NOT NULL CHECK (position >= 0),"
    "    fault_index      INTEGER NOT NULL,"
    "    result_code      INTEGER NOT NULL CHECK (result_code BETWEEN 0 AND 6),"
    "    reason           TEXT,"
    "    details          TEXT,"
    "    source_file      TEXT,"
    "    source_line      INTEGER,"
    "    resource_address INTEGER,"
    "    UNIQUE (result_id, position)"
    ");",

    // Indexes for the foreign keys that do not lead a UNIQUE constraint, and for failure
    // lookups. idx_runs_suite also finds a suite's latest run. result_code > 1 selects
    // every result worse than FL_PASS.
    "CREATE INDEX idx_runs_suite ON runs (suite_id, id);",
    "CREATE INDEX idx_cases_baseline_run ON cases (baseline_run_id);",
    "CREATE INDEX idx_results_case ON results (case_id, run_id);",
    "CREATE INDEX idx_results_failed ON results (run_id) WHERE result_code > 1;",

    // Totals per run. Result codes: FL_PASS 1, FL_LEAK 2, FL_INVALID_FREE 3,
    // FL_DOUBLE_FREE 4; failed counts every code above FL_LEAK. Failure types:
    // FL_SETUP_FAILURE 1, FL_CLEANUP_FAILURE 3. pass_rate is NULL for a run with no
    // results.
    "CREATE VIEW run_totals AS "
    "SELECT r.id AS run_id,"
    "       r.suite_id,"
    "       r.started_at,"
    "       r.completed_at,"
    "       r.case_count,"
    "       count(x.id) AS cases_run,"
    "       coalesce(sum(x.result_code = 1), 0) AS passed,"
    "       coalesce(sum(x.result_code = 2), 0) AS passed_with_leaks,"
    "       coalesce(sum(x.result_code > 2), 0) AS failed,"
    "       coalesce(sum(x.failure_type = 1), 0) AS setup_failures,"
    "       coalesce(sum(x.failure_type = 3), 0) AS cleanup_failures,"
    "       coalesce(sum(x.result_code IN (3, 4)), 0) AS invalid_frees,"
    "       coalesce(sum(x.fault_sites), 0) AS fault_sites,"
    "       coalesce(sum(x.elapsed_seconds), 0.0) AS elapsed_seconds,"
    "       coalesce(sum(x.discovery_failures), 0) AS discovery_failures,"
    "       coalesce(sum(x.injection_failures), 0) AS injection_failures,"
    "       CAST(sum(x.result_code = 1) AS REAL) / nullif(count(x.id), 0) AS pass_rate "
    "FROM runs r "
    "LEFT JOIN results x ON x.run_id = r.id "
    "GROUP BY r.id;",

    NULL // Terminator
};

// schema_steps[i] takes a database from version i to version i + 1.
static char const *const *const schema_steps[] = {
    schema_v1,
};

FL_STATIC_ASSERT(FL_ARRAY_COUNT(schema_steps) == (size_t)FL_SCHEMA_VERSION,
                 "one schema step per schema version");

// Run one SQL batch; on failure, throw `reason` with the SQLite error and the head of the
// statement.
static void sql_exec_or_throw(sqlite3 *db, char const *sql, FLExceptionReason reason) {
    char *errmsg = NULL;
    int   rc     = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        char details[256];
        snprintf(details, sizeof details, "%s in \"%.80s\"",
                 errmsg ? errmsg : sqlite3_errstr(rc), sql);
        sqlite3_free(errmsg); // free before the throw
        LOG_ERROR(faultline_db, "%s", details);
        FL_THROW_DETAILS(reason, "sqlite3: %s", details);
    }
}

/**
 * @brief Bring a database from version @p from to FL_SCHEMA_VERSION in one transaction.
 *
 * The version is set inside the same transaction as the schema steps. A step that throws
 * leaves the transaction open; the caller closes the connection, which rolls it back, so
 * the file keeps neither the step's objects nor a new version.
 */
static void schema_upgrade(sqlite3 *db, int from) {
    sql_exec_or_throw(db, "BEGIN IMMEDIATE;", faultline_db_create_failed);

    for (int step = from; step < FL_SCHEMA_VERSION; step++) {
        for (char const *const *sql = schema_steps[step]; *sql != NULL; sql++) {
            sql_exec_or_throw(db, *sql, faultline_db_create_failed);
        }
    }

    char set_version[40];
    snprintf(set_version, sizeof set_version, "PRAGMA user_version = %d;",
             FL_SCHEMA_VERSION);
    sql_exec_or_throw(db, set_version, faultline_db_create_failed);

    sql_exec_or_throw(db, "COMMIT;", faultline_db_create_failed);
}

/**
 * @brief Open a results database, creating or upgrading its schema.
 *
 * @param db_path Path to SQLite database file
 * @return sqlite3* Database connection handle
 * @throw faultline_db_create_failed if the file cannot be opened or its schema written
 * @throw faultline_db_version_too_new if a newer build wrote the file
 * @throw faultline_db_legacy_schema if the file holds the schema that preceded version 1
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

    FL_TRY {
        // Foreign key enforcement is per connection, and the pragma has no effect inside
        // a transaction, so it runs before the upgrade begins.
        sql_exec_or_throw(db, "PRAGMA foreign_keys = ON;", faultline_db_create_failed);

        int const version
            = db_query_int(db, "PRAGMA user_version;", -1, "schema version query");
        int const tables
            = db_query_int(db, "SELECT count(*) FROM sqlite_master WHERE type = 'table';",
                           -1, "table count query");
        if (version < 0 || tables < 0) {
            FL_THROW_DETAILS(faultline_db_create_failed, "cannot read the schema of '%s'",
                             db_path);
        }

        // Both refusals happen before anything below writes to the file.
        if (version > FL_SCHEMA_VERSION) {
            FL_THROW_DETAILS(faultline_db_version_too_new,
                             "'%s' is schema v%d; this build supports v%d", db_path,
                             version, FL_SCHEMA_VERSION);
        }

        // The schema that preceded version 1 never set user_version, so it reads as 0.
        if (version == 0 && tables > 0) {
            FL_THROW_DETAILS(faultline_db_legacy_schema,
                             "'%s' predates schema v1; delete it or pass --db with a "
                             "new path",
                             db_path);
        }

        /*
         * The goal is to balance the number of writes to disk vs database durability
         * across an OS crash or power loss. We select the write-ahead log (WAL) instead of
         * the default rollback journal, because it is much faster. This PRAGMA pair also
         * reduces the number of writes that this module still leaves auto-committed.
         *
         * NORMAL synchronization trades a small amount of durability across an OS crash
         * or power loss for a significant performance gain. NORMAL only fsyncs at WAL
         * checkpoints instead of every commit, while the default, FULL, fsyncs twice per
         * commit. An ordinary process crash is still safe, and this database holds
         * reproducible test results, not data worth an fsync-per-commit guarantee.
         */
        db_exec(db, "PRAGMA journal_mode = WAL;", "WAL journal mode");
        db_exec(db, "PRAGMA synchronous = NORMAL;", "relaxed synchronous mode");

        if (version < FL_SCHEMA_VERSION) {
            schema_upgrade(db, version);
        }
        LOG_VERBOSE(faultline_db, "Database initialized successfully: %s", db_path);
    }
    FL_CATCH_ALL_RETHROW {
        LOG_ERROR(faultline_db, "Exception during database initialization");
        // Closing the connection also rolls back an unfinished upgrade.
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
