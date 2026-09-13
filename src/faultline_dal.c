/**
 * @file faultline_dal.c
 * @author Douglas Cuthbertson
 * @brief Data-access layer over the results database: exception reasons and statement
 * helpers, followed by the domain slices.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Two conventions govern every slice:
 *
 * 1. A function that prepares a statement holds it in a volatile local, finalizes it in
 *    FL_CATCH_ALL_RETHROW, and finalizes it again on the way out. sqlite3_finalize(NULL)
 *    does nothing, so the two paths need no flag between them. The local is volatile
 *    because the handler reads it after a longjmp (C17 7.13.2.1p3).
 * 2. A write that touches more than one table runs inside a named SAVEPOINT, so a throw
 *    partway through leaves none of the write behind.
 */
#include <faultline_dal.h>
#include <faultline_sqlite.h>               // FL_DB_TIME_FORMAT
#include <faultline/fault.h>                // Fault, fault_buffer_count, fault_buffer_get
#include <faultline/fl_abbreviated_types.h> // i64
#include <faultline/fl_context.h>           // FLContext, faultline_get_results_count
#include <faultline/fl_log.h>               // LOG_ERROR, LOG_VERBOSE
#include <faultline/fl_test.h>              // FLTestSuite
#include <faultline/fl_test_summary.h>      // FLTestSummary
#include <faultline/fl_try.h>               // FL_TRY, FL_CATCH_ALL_RETHROW, FL_THROW_DETAILS
#include <faultline/fl_types.h>             // FL_FAILURE_NONE
#include <sqlite/sqlite3.h>                 // sqlite3_*

#include <stdint.h> // uintptr_t
#include <time.h>   // time_t

#if defined(__cplusplus)
extern "C" {
#endif

FLExceptionReason faultline_dal_read_failed  = "database read failed";
FLExceptionReason faultline_dal_write_failed = "database write failed";

#if defined(__cplusplus)
}
#endif

static char const *dal_module = "Faultline DAL";

// ---------------------------------------------------------------------------------------
// Statement helpers
//
// Each throws `reason` with SQLite's error message. dal_prepare_or_throw returns the
// statement rather than filling an out-parameter, so a caller can assign it straight into
// a `sqlite3_stmt *volatile`.
// ---------------------------------------------------------------------------------------

// Log the connection's last error and throw it as `reason`.
static void dal_throw(sqlite3 *db, FLExceptionReason reason) {
    char const *details = sqlite3_errmsg(db);
    LOG_ERROR(dal_module, "%s", details);
    FL_THROW_DETAILS(reason, "sqlite3: %s", details);
}

// Run SQL that returns no rows.
static void dal_exec_or_throw(sqlite3 *db, char const *sql, FLExceptionReason reason) {
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        dal_throw(db, reason);
    }
}

static sqlite3_stmt *dal_prepare_or_throw(sqlite3 *db, char const *sql,
                                          FLExceptionReason reason) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        // prepare leaves stmt NULL on failure, so there is nothing to finalize.
        dal_throw(db, reason);
    }
    return stmt;
}

// Step a statement that returns no rows.
static void dal_step_done_or_throw(sqlite3 *db, sqlite3_stmt *stmt,
                                   FLExceptionReason reason) {
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        dal_throw(db, reason);
    }
}

// Step a statement ending in RETURNING id, and return the id. The write happens during
// this first step.
static i64 dal_step_id_or_throw(sqlite3 *db, sqlite3_stmt *stmt, FLExceptionReason reason) {
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        dal_throw(db, reason);
    }
    return sqlite3_column_int64(stmt, 0);
}

// Distinguish a walk that ended from one that broke. A visitor that returns false leaves
// the last step result at SQLITE_ROW, which is not an error.
static void dal_check_walk_end(sqlite3 *db, int rc, FLExceptionReason reason) {
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        dal_throw(db, reason);
    }
}

// Bind text, or SQL NULL when value is NULL. SQLITE_TRANSIENT copies the bytes, so the
// caller's string need not outlive the step. A NULL filter value is how a read's
// (?N IS NULL OR column = ?N) predicate matches every row.
static void dal_bind_text(sqlite3 *db, sqlite3_stmt *stmt, int index, char const *value,
                          FLExceptionReason reason) {
    int rc = (value == NULL)
                 ? sqlite3_bind_null(stmt, index)
                 : sqlite3_bind_text(stmt, index, value, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        dal_throw(db, reason);
    }
}

static void dal_bind_i64(sqlite3 *db, sqlite3_stmt *stmt, int index, i64 value,
                         FLExceptionReason reason) {
    if (sqlite3_bind_int64(stmt, index, value) != SQLITE_OK) {
        dal_throw(db, reason);
    }
}

static void dal_bind_double(sqlite3 *db, sqlite3_stmt *stmt, int index, double value,
                            FLExceptionReason reason) {
    if (sqlite3_bind_double(stmt, index, value) != SQLITE_OK) {
        dal_throw(db, reason);
    }
}

static void dal_bind_null(sqlite3 *db, sqlite3_stmt *stmt, int index,
                          FLExceptionReason reason) {
    if (sqlite3_bind_null(stmt, index) != SQLITE_OK) {
        dal_throw(db, reason);
    }
}

// Bind a read's :limit parameter. SQLite reads a negative LIMIT as no limit, so a limit
// of 0 or less binds -1.
static void dal_bind_limit(sqlite3 *db, sqlite3_stmt *stmt, i64 limit,
                           FLExceptionReason reason) {
    int const index = sqlite3_bind_parameter_index(stmt, ":limit");
    dal_bind_i64(db, stmt, index, (limit > 0) ? limit : -1, reason);
}

// A text column, or NULL for SQL NULL. The pointer is valid until the statement's next
// step, reset, or finalize.
static char const *dal_column_text(sqlite3_stmt *stmt, int column) {
    return (char const *)sqlite3_column_text(stmt, column);
}

// NULL for a missing or empty string, so an absent value is stored as NULL, never "".
static char const *dal_text_or_null(char const *text) {
    return (text != NULL && text[0] != '\0') ? text : NULL;
}

#include "faultline_dal_runs.c"
#include "faultline_dal_suites.c"
#include "faultline_dal_cases.c"
#include "faultline_dal_faults.c"
