/**
 * @file faultline_dal_runs.c
 * @author Douglas Cuthbertson
 * @brief Runs slice of the data-access layer: record a finished suite run, and read
 * runs, their results, their failures, and their daily trends.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit. #included by faultline_dal.c after its statement
 * helpers.
 */

// ---------------------------------------------------------------------------------------
// Recording
// ---------------------------------------------------------------------------------------

// Parameter indexes of dal_result_insert_sql.
enum {
    DAL_RESULT_RUN_ID = 1,
    DAL_RESULT_CASE_ID,
    DAL_RESULT_CASE_INDEX,
    DAL_RESULT_CODE,
    DAL_RESULT_FAILURE_TYPE,
    DAL_RESULT_FAILURE_PHASE,
    DAL_RESULT_REASON,
    DAL_RESULT_DETAILS,
    DAL_RESULT_ELAPSED_SECONDS,
    DAL_RESULT_DISCOVERY_SECONDS,
    DAL_RESULT_INJECTION_SECONDS,
    DAL_RESULT_FAULT_SITES,
    DAL_RESULT_DISCOVERY_FAILURES,
    DAL_RESULT_INJECTION_FAILURES
};

// Parameter indexes of dal_fault_insert_sql.
enum {
    DAL_FAULT_RESULT_ID = 1,
    DAL_FAULT_POSITION,
    DAL_FAULT_INDEX,
    DAL_FAULT_CODE,
    DAL_FAULT_REASON,
    DAL_FAULT_DETAILS,
    DAL_FAULT_SOURCE_FILE,
    DAL_FAULT_SOURCE_LINE,
    DAL_FAULT_RESOURCE
};

// A new case takes this run as its baseline. The no-op update on a case that already
// exists leaves its baseline alone and lets RETURNING report its id.
static char const *const dal_case_upsert_sql
    = "INSERT INTO cases (suite_id, name, baseline_run_id, baseline_set_at)"
      " VALUES (?1, ?2, ?3, strftime('" FL_DB_TIME_FORMAT "', 'now'))"
      " ON CONFLICT (suite_id, name) DO UPDATE SET name = excluded.name"
      " RETURNING id;";

static char const *const dal_result_insert_sql
    = "INSERT INTO results (run_id, case_id, case_index, result_code, failure_type,"
      " failure_phase, reason, details, elapsed_seconds, discovery_seconds,"
      " injection_seconds, fault_sites, discovery_failures, injection_failures)"
      " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14)"
      " RETURNING id;";

static char const *const dal_fault_insert_sql
    = "INSERT INTO faults (result_id, position, fault_index, result_code, reason,"
      " details, source_file, source_line, resource_address)"
      " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);";

// Insert the suite if it is new, and return its id either way. The no-op update lets
// RETURNING report the id of a suite that already exists.
static i64 dal_suite_upsert(sqlite3 *db, char const *name) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW
    i64                    id   = 0;

    FL_TRY {
        stmt = dal_prepare_or_throw(db,
                                    "INSERT INTO suites (name) VALUES (?1)"
                                    " ON CONFLICT (name) DO UPDATE SET name = excluded.name"
                                    " RETURNING id;",
                                    faultline_dal_write_failed);
        dal_bind_text(db, stmt, 1, name, faultline_dal_write_failed);
        id = dal_step_id_or_throw(db, stmt, faultline_dal_write_failed);
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(stmt);
    return id;
}

// Insert the run and return its id. A start time of 0 means the context was never
// stamped, so the current time is recorded as the start instead.
static i64 dal_run_insert(sqlite3 *db, i64 suite_id, time_t started, size_t case_count) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW
    i64                    id   = 0;

    FL_TRY {
        stmt = dal_prepare_or_throw(
            db,
            "INSERT INTO runs (suite_id, started_at, completed_at, case_count)"
            " VALUES (?1,"
            " coalesce(strftime('" FL_DB_TIME_FORMAT "', ?2, 'unixepoch'),"
            "          strftime('" FL_DB_TIME_FORMAT "', 'now')),"
            " strftime('" FL_DB_TIME_FORMAT "', 'now'), ?3)"
            " RETURNING id;",
            faultline_dal_write_failed);
        dal_bind_i64(db, stmt, 1, suite_id, faultline_dal_write_failed);
        if (started == 0) {
            dal_bind_null(db, stmt, 2, faultline_dal_write_failed);
        } else {
            dal_bind_i64(db, stmt, 2, (i64)started, faultline_dal_write_failed);
        }
        dal_bind_i64(db, stmt, 3, (i64)case_count, faultline_dal_write_failed);
        id = dal_step_id_or_throw(db, stmt, faultline_dal_write_failed);
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(stmt);
    return id;
}

// The name of the case a summary belongs to.
static char const *dal_case_name(FLTestSuite const *ts, FLTestSummary const *summary) {
    if (summary->index >= ts->count) {
        LOG_ERROR(dal_module, "suite '%s' has no case %u", ts->name, summary->index);
        FL_THROW_DETAILS(faultline_dal_write_failed, "suite '%s' has no case %u", ts->name,
                         summary->index);
    }
    return ts->test_cases[summary->index]->name;
}

// Insert the case if the suite has not recorded it before, and return its id either way.
static i64 dal_case_upsert(sqlite3 *db, sqlite3_stmt *stmt, i64 suite_id, char const *name,
                           i64 run_id) {
    sqlite3_reset(stmt);
    dal_bind_i64(db, stmt, 1, suite_id, faultline_dal_write_failed);
    dal_bind_text(db, stmt, 2, name, faultline_dal_write_failed);
    dal_bind_i64(db, stmt, 3, run_id, faultline_dal_write_failed);
    return dal_step_id_or_throw(db, stmt, faultline_dal_write_failed);
}

// Insert one case's result and return its id. Summaries carry distinct case indexes, so
// a UNIQUE failure here means two of the suite's cases share a name.
static i64 dal_result_insert(sqlite3 *db, sqlite3_stmt *stmt, i64 run_id, i64 case_id,
                             FLTestSummary const *summary, char const *suite_name,
                             char const *case_name) {
    FLExceptionReason const reason = faultline_dal_write_failed;

    sqlite3_reset(stmt);
    dal_bind_i64(db, stmt, DAL_RESULT_RUN_ID, run_id, reason);
    dal_bind_i64(db, stmt, DAL_RESULT_CASE_ID, case_id, reason);
    dal_bind_i64(db, stmt, DAL_RESULT_CASE_INDEX, summary->index, reason);
    dal_bind_i64(db, stmt, DAL_RESULT_CODE, summary->code, reason);
    dal_bind_i64(db, stmt, DAL_RESULT_FAILURE_TYPE, summary->failure_type, reason);
    if (summary->failure_type == FL_FAILURE_NONE) {
        dal_bind_null(db, stmt, DAL_RESULT_FAILURE_PHASE, reason);
    } else {
        dal_bind_i64(db, stmt, DAL_RESULT_FAILURE_PHASE, summary->failure_phase, reason);
    }
    dal_bind_text(db, stmt, DAL_RESULT_REASON, dal_text_or_null(summary->reason), reason);
    dal_bind_text(db, stmt, DAL_RESULT_DETAILS, dal_text_or_null(summary->details), reason);
    dal_bind_double(db, stmt, DAL_RESULT_ELAPSED_SECONDS, summary->elapsed_seconds, reason);
    dal_bind_double(db, stmt, DAL_RESULT_DISCOVERY_SECONDS, summary->discovery_time,
                    reason);
    dal_bind_double(db, stmt, DAL_RESULT_INJECTION_SECONDS, summary->injection_time,
                    reason);
    dal_bind_i64(db, stmt, DAL_RESULT_FAULT_SITES, summary->faults_exercised, reason);
    dal_bind_i64(db, stmt, DAL_RESULT_DISCOVERY_FAILURES, summary->discovery_failures,
                 reason);
    dal_bind_i64(db, stmt, DAL_RESULT_INJECTION_FAILURES, summary->injection_failures,
                 reason);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        if (sqlite3_extended_errcode(db) == SQLITE_CONSTRAINT_UNIQUE) {
            LOG_ERROR(dal_module, "suite '%s' has more than one case named '%s'",
                      suite_name, case_name);
            FL_THROW_DETAILS(reason, "suite '%s' has more than one case named '%s'",
                             suite_name, case_name);
        }
        dal_throw(db, reason);
    }

    return sqlite3_column_int64(stmt, 0);
}

// Insert a result's faults in the order the summary holds them. A fault with no source
// file has no line, and a fault with no resource has no address.
static void dal_faults_insert(sqlite3 *db, sqlite3_stmt *stmt, i64 result_id,
                              FLTestSummary *summary) {
    FLExceptionReason const reason = faultline_dal_write_failed;
    size_t const            count  = fault_buffer_count(&summary->fault_buffer);

    for (size_t position = 0; position < count; position++) {
        Fault const *fault = fault_buffer_get(&summary->fault_buffer, position);
        char const  *file  = dal_text_or_null(fault->file);

        sqlite3_reset(stmt);
        dal_bind_i64(db, stmt, DAL_FAULT_RESULT_ID, result_id, reason);
        dal_bind_i64(db, stmt, DAL_FAULT_POSITION, (i64)position, reason);
        dal_bind_i64(db, stmt, DAL_FAULT_INDEX, fault->index, reason);
        dal_bind_i64(db, stmt, DAL_FAULT_CODE, fault->code, reason);
        dal_bind_text(db, stmt, DAL_FAULT_REASON, dal_text_or_null(fault->reason), reason);
        dal_bind_text(db, stmt, DAL_FAULT_DETAILS, dal_text_or_null(fault->details), reason);
        dal_bind_text(db, stmt, DAL_FAULT_SOURCE_FILE, file, reason);

        if (file == NULL) {
            dal_bind_null(db, stmt, DAL_FAULT_SOURCE_LINE, reason);
        } else {
            dal_bind_i64(db, stmt, DAL_FAULT_SOURCE_LINE, fault->line, reason);
        }

        if (fault->resource == NULL) {
            dal_bind_null(db, stmt, DAL_FAULT_RESOURCE, reason);
        } else {
            dal_bind_i64(db, stmt, DAL_FAULT_RESOURCE, (i64)(uintptr_t)fault->resource,
                         reason);
        }

        dal_step_done_or_throw(db, stmt, reason);
    }
}

// Record each summary's case, result, and faults. The three statements are prepared once
// and reset for each row.
static void dal_results_insert(sqlite3 *db, i64 suite_id, i64 run_id, FLContext *fctx) {
    sqlite3_stmt *volatile case_stmt   = NULL; // volatile: read in FL_CATCH_ALL_RETHROW
    sqlite3_stmt *volatile result_stmt = NULL;
    sqlite3_stmt *volatile fault_stmt  = NULL;

    FL_TRY {
        case_stmt = dal_prepare_or_throw(db, dal_case_upsert_sql, faultline_dal_write_failed);
        result_stmt
            = dal_prepare_or_throw(db, dal_result_insert_sql, faultline_dal_write_failed);
        fault_stmt
            = dal_prepare_or_throw(db, dal_fault_insert_sql, faultline_dal_write_failed);

        size_t const count = faultline_get_results_count(fctx);
        for (size_t i = 0; i < count; i++) {
            FLTestSummary *summary = faultline_test_summary_buffer_get(&fctx->results, i);
            char const    *name    = dal_case_name(fctx->ts, summary);

            i64 const case_id   = dal_case_upsert(db, case_stmt, suite_id, name, run_id);
            i64 const result_id = dal_result_insert(db, result_stmt, run_id, case_id,
                                                    summary, fctx->ts->name, name);
            dal_faults_insert(db, fault_stmt, result_id, summary);
        }
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(case_stmt);
        sqlite3_finalize(result_stmt);
        sqlite3_finalize(fault_stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(case_stmt);
    sqlite3_finalize(result_stmt);
    sqlite3_finalize(fault_stmt);
}

i64 faultline_run_record(sqlite3 *db, FLContext *fctx) {
    FLTestSuite const *ts     = fctx->ts;
    i64                run_id = 0;

    // Several tables, so one savepoint: a throw partway through must not leave a run
    // whose results were never written.
    dal_exec_or_throw(db, "SAVEPOINT faultline_run_record;", faultline_dal_write_failed);
    FL_TRY {
        i64 const suite_id = dal_suite_upsert(db, ts->name);
        run_id             = dal_run_insert(db, suite_id, fctx->run_start_time, ts->count);
        dal_results_insert(db, suite_id, run_id, fctx);
    }
    FL_CATCH_ALL_RETHROW {
        // Undo every write and drop the savepoint. The result is ignored: the exception
        // being rethrown is the more useful report.
        sqlite3_exec(db, "ROLLBACK TO faultline_run_record; RELEASE faultline_run_record;",
                     NULL, NULL, NULL);
    }
    FL_END_TRY;
    dal_exec_or_throw(db, "RELEASE faultline_run_record;", faultline_dal_write_failed);

    LOG_VERBOSE(dal_module, "Recorded run %lld of suite %s", (long long)run_id, ts->name);
    return run_id;
}

// ---------------------------------------------------------------------------------------
// Runs
// ---------------------------------------------------------------------------------------

// The columns dal_run_row reads, in its order.
#define DAL_RUN_SELECT                                                                   \
    "SELECT t.run_id, s.name, t.started_at, t.case_count, t.cases_run, t.passed,"       \
    " t.fault_sites, t.elapsed_seconds, t.pass_rate, t.discovery_failures,"             \
    " t.injection_failures"                                                              \
    " FROM run_totals t JOIN suites s ON s.id = t.suite_id"

enum {
    DAL_RUN_COL_ID = 0,
    DAL_RUN_COL_SUITE_NAME,
    DAL_RUN_COL_STARTED_AT,
    DAL_RUN_COL_CASE_COUNT,
    DAL_RUN_COL_CASES_RUN,
    DAL_RUN_COL_PASSED,
    DAL_RUN_COL_FAULT_SITES,
    DAL_RUN_COL_ELAPSED_SECONDS,
    DAL_RUN_COL_PASS_RATE,
    DAL_RUN_COL_DISCOVERY_FAILURES,
    DAL_RUN_COL_INJECTION_FAILURES
};

// A pass rate of SQL NULL, for a run with no results, reads as 0.
static FLRunRow dal_run_row(sqlite3_stmt *stmt) {
    FLRunRow row = {
        .id                 = sqlite3_column_int64(stmt, DAL_RUN_COL_ID),
        .suite_name         = dal_column_text(stmt, DAL_RUN_COL_SUITE_NAME),
        .started_at         = dal_column_text(stmt, DAL_RUN_COL_STARTED_AT),
        .case_count         = sqlite3_column_int64(stmt, DAL_RUN_COL_CASE_COUNT),
        .cases_run          = sqlite3_column_int64(stmt, DAL_RUN_COL_CASES_RUN),
        .passed             = sqlite3_column_int64(stmt, DAL_RUN_COL_PASSED),
        .fault_sites        = sqlite3_column_int64(stmt, DAL_RUN_COL_FAULT_SITES),
        .elapsed_seconds    = sqlite3_column_double(stmt, DAL_RUN_COL_ELAPSED_SECONDS),
        .pass_rate          = sqlite3_column_double(stmt, DAL_RUN_COL_PASS_RATE),
        .discovery_failures = sqlite3_column_int64(stmt, DAL_RUN_COL_DISCOVERY_FAILURES),
        .injection_failures = sqlite3_column_int64(stmt, DAL_RUN_COL_INJECTION_FAILURES),
    };

    return row;
}

void faultline_run_list(sqlite3 *db, char const *suite_name, i64 limit,
                        FLRunVisitor *visit, void *user) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW

    FL_TRY {
        stmt = dal_prepare_or_throw(db,
                                    DAL_RUN_SELECT " WHERE (?1 IS NULL OR s.name = ?1)"
                                                   " ORDER BY t.run_id DESC LIMIT :limit;",
                                    faultline_dal_read_failed);
        dal_bind_text(db, stmt, 1, suite_name, faultline_dal_read_failed);
        dal_bind_limit(db, stmt, limit, faultline_dal_read_failed);

        int rc = SQLITE_DONE;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            FLRunRow const row = dal_run_row(stmt);
            if (!visit(&row, user)) {
                break;
            }
        }
        dal_check_walk_end(db, rc, faultline_dal_read_failed);
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(stmt);
}

bool faultline_run_find(sqlite3 *db, i64 run_id, FLRunVisitor *visit, void *user) {
    sqlite3_stmt *volatile stmt  = NULL; // volatile: read in FL_CATCH_ALL_RETHROW
    bool                   found = false;

    FL_TRY {
        stmt = dal_prepare_or_throw(db, DAL_RUN_SELECT " WHERE t.run_id = ?1;",
                                    faultline_dal_read_failed);
        dal_bind_i64(db, stmt, 1, run_id, faultline_dal_read_failed);

        int const rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            FLRunRow const row = dal_run_row(stmt);
            found              = true;
            visit(&row, user);
        } else {
            dal_check_walk_end(db, rc, faultline_dal_read_failed);
        }
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(stmt);
    return found;
}

// ---------------------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------------------

enum {
    DAL_RESULT_COL_CASE_NAME = 0,
    DAL_RESULT_COL_CODE,
    DAL_RESULT_COL_ELAPSED_SECONDS,
    DAL_RESULT_COL_FAULT_SITES,
    DAL_RESULT_COL_REASON,
    DAL_RESULT_COL_DETAILS,
    DAL_RESULT_COL_SOURCE_FILE,
    DAL_RESULT_COL_SOURCE_LINE
};

void faultline_result_list(sqlite3 *db, i64 run_id, FLResultVisitor *visit, void *user) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW

    FL_TRY {
        // A result's first fault is the one at position 0.
        stmt = dal_prepare_or_throw(
            db,
            "SELECT c.name, x.result_code, x.elapsed_seconds, x.fault_sites, x.reason,"
            " x.details, f.source_file, f.source_line"
            " FROM results x"
            " JOIN cases c ON c.id = x.case_id"
            " LEFT JOIN faults f ON f.result_id = x.id AND f.position = 0"
            " WHERE x.run_id = ?1"
            " ORDER BY x.case_index;",
            faultline_dal_read_failed);
        dal_bind_i64(db, stmt, 1, run_id, faultline_dal_read_failed);

        int rc = SQLITE_DONE;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            FLResultRow const row = {
                .case_name   = dal_column_text(stmt, DAL_RESULT_COL_CASE_NAME),
                .result_code = (FLResultCode)sqlite3_column_int(stmt, DAL_RESULT_COL_CODE),
                .elapsed_seconds
                = sqlite3_column_double(stmt, DAL_RESULT_COL_ELAPSED_SECONDS),
                .fault_sites = sqlite3_column_int64(stmt, DAL_RESULT_COL_FAULT_SITES),
                .reason      = dal_column_text(stmt, DAL_RESULT_COL_REASON),
                .details     = dal_column_text(stmt, DAL_RESULT_COL_DETAILS),
                .source_file = dal_column_text(stmt, DAL_RESULT_COL_SOURCE_FILE),
                .source_line = sqlite3_column_int(stmt, DAL_RESULT_COL_SOURCE_LINE),
            };
            if (!visit(&row, user)) {
                break;
            }
        }
        dal_check_walk_end(db, rc, faultline_dal_read_failed);
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(stmt);
}

// ---------------------------------------------------------------------------------------
// Failures
// ---------------------------------------------------------------------------------------

enum {
    DAL_FAILURE_COL_RUN_ID = 0,
    DAL_FAILURE_COL_SUITE_NAME,
    DAL_FAILURE_COL_CASE_NAME,
    DAL_FAILURE_COL_CODE,
    DAL_FAILURE_COL_REASON,
    DAL_FAILURE_COL_DETAILS,
    DAL_FAILURE_COL_SOURCE_FILE,
    DAL_FAILURE_COL_SOURCE_LINE,
    DAL_FAILURE_COL_RESOURCE
};

void faultline_failure_list(sqlite3 *db, char const *suite_name, i64 limit,
                            FLFailureScope scope, FLFailureVisitor *visit, void *user) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW

    FL_TRY {
        // result_code > 1 selects results worse than FL_PASS. ?2 is 1 to read every run.
        // Otherwise a suite's latest run is its largest run id: ids never repeat, while
        // two runs can start in the same second.
        stmt = dal_prepare_or_throw(
            db,
            "SELECT r.id, s.name, c.name, x.result_code, x.reason, x.details,"
            " f.source_file, f.source_line, f.resource_address"
            " FROM results x"
            " JOIN runs r ON r.id = x.run_id"
            " JOIN suites s ON s.id = r.suite_id"
            " JOIN cases c ON c.id = x.case_id"
            " LEFT JOIN faults f ON f.result_id = x.id"
            " WHERE (?1 IS NULL OR s.name = ?1)"
            " AND x.result_code > 1"
            " AND (?2 = 1"
            "      OR r.id = (SELECT max(r2.id) FROM runs r2 WHERE r2.suite_id = r.suite_id))"
            " ORDER BY r.id DESC, x.case_index, f.position"
            " LIMIT :limit;",
            faultline_dal_read_failed);
        dal_bind_text(db, stmt, 1, suite_name, faultline_dal_read_failed);
        dal_bind_i64(db, stmt, 2, scope == FL_FAILURES_ALL, faultline_dal_read_failed);
        dal_bind_limit(db, stmt, limit, faultline_dal_read_failed);

        int rc = SQLITE_DONE;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            FLFailureRow const row = {
                .run_id      = sqlite3_column_int64(stmt, DAL_FAILURE_COL_RUN_ID),
                .suite_name  = dal_column_text(stmt, DAL_FAILURE_COL_SUITE_NAME),
                .case_name   = dal_column_text(stmt, DAL_FAILURE_COL_CASE_NAME),
                .result_code = (FLResultCode)sqlite3_column_int(stmt, DAL_FAILURE_COL_CODE),
                .reason      = dal_column_text(stmt, DAL_FAILURE_COL_REASON),
                .details     = dal_column_text(stmt, DAL_FAILURE_COL_DETAILS),
                .source_file = dal_column_text(stmt, DAL_FAILURE_COL_SOURCE_FILE),
                .source_line = sqlite3_column_int(stmt, DAL_FAILURE_COL_SOURCE_LINE),
                .resource_address = sqlite3_column_int64(stmt, DAL_FAILURE_COL_RESOURCE),
            };
            if (!visit(&row, user)) {
                break;
            }
        }
        dal_check_walk_end(db, rc, faultline_dal_read_failed);
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(stmt);
}

// ---------------------------------------------------------------------------------------
// Trends
// ---------------------------------------------------------------------------------------

enum {
    DAL_TREND_COL_SUITE_NAME = 0,
    DAL_TREND_COL_DAY,
    DAL_TREND_COL_RUNS,
    DAL_TREND_COL_PASS_RATE,
    DAL_TREND_COL_ELAPSED_SECONDS,
    DAL_TREND_COL_FAILURES,
    DAL_TREND_COL_HAS_PREVIOUS,
    DAL_TREND_COL_PREVIOUS_PASS_RATE,
    DAL_TREND_COL_PREVIOUS_ELAPSED_SECONDS,
    DAL_TREND_COL_PREVIOUS_FAILURES
};

void faultline_trend_list(sqlite3 *db, char const *suite_name, i64 limit,
                          FLTrendVisitor *visit, void *user) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW

    FL_TRY {
        // The inner query totals each suite's runs per day; lag() then attaches the
        // suite's previous day. lag(day) is NULL only on the suite's first day.
        stmt = dal_prepare_or_throw(
            db,
            "SELECT suite_name, day, runs, pass_rate, elapsed_seconds, failures,"
            " lag(day) OVER w IS NOT NULL,"
            " lag(pass_rate) OVER w, lag(elapsed_seconds) OVER w, lag(failures) OVER w"
            " FROM ("
            "   SELECT s.name AS suite_name, date(t.started_at) AS day, count(*) AS runs,"
            "          avg(t.pass_rate) AS pass_rate,"
            "          avg(t.elapsed_seconds) AS elapsed_seconds,"
            "          sum(t.failed) AS failures"
            "   FROM run_totals t JOIN suites s ON s.id = t.suite_id"
            "   WHERE (?1 IS NULL OR s.name = ?1)"
            "   GROUP BY s.name, day"
            " )"
            " WINDOW w AS (PARTITION BY suite_name ORDER BY day)"
            " ORDER BY suite_name, day DESC"
            " LIMIT :limit;",
            faultline_dal_read_failed);
        dal_bind_text(db, stmt, 1, suite_name, faultline_dal_read_failed);
        dal_bind_limit(db, stmt, limit, faultline_dal_read_failed);

        int rc = SQLITE_DONE;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            FLTrendRow const row = {
                .suite_name      = dal_column_text(stmt, DAL_TREND_COL_SUITE_NAME),
                .day             = dal_column_text(stmt, DAL_TREND_COL_DAY),
                .runs            = sqlite3_column_int64(stmt, DAL_TREND_COL_RUNS),
                .pass_rate       = sqlite3_column_double(stmt, DAL_TREND_COL_PASS_RATE),
                .elapsed_seconds = sqlite3_column_double(stmt, DAL_TREND_COL_ELAPSED_SECONDS),
                .failures        = sqlite3_column_int64(stmt, DAL_TREND_COL_FAILURES),
                .has_previous    = sqlite3_column_int(stmt, DAL_TREND_COL_HAS_PREVIOUS) != 0,
                .previous_pass_rate
                = sqlite3_column_double(stmt, DAL_TREND_COL_PREVIOUS_PASS_RATE),
                .previous_elapsed_seconds
                = sqlite3_column_double(stmt, DAL_TREND_COL_PREVIOUS_ELAPSED_SECONDS),
                .previous_failures
                = sqlite3_column_int64(stmt, DAL_TREND_COL_PREVIOUS_FAILURES),
            };
            if (!visit(&row, user)) {
                break;
            }
        }
        dal_check_walk_end(db, rc, faultline_dal_read_failed);
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(stmt);
}
