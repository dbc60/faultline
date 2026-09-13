/**
 * @file faultline_dal_suites.c
 * @author Douglas Cuthbertson
 * @brief Suites slice of the data-access layer.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit. #included by faultline_dal.c after its statement
 * helpers.
 */

// The columns dal_suite_row reads, in its order. A suite without runs still has a row:
// the LEFT JOIN gives it NULL totals, which read as 0, and NULL first and last runs.
#define DAL_SUITE_SELECT                                                                 \
    "SELECT s.name, count(t.run_id),"                                                    \
    " (SELECT count(*) FROM cases c WHERE c.suite_id = s.id),"                           \
    " min(t.started_at), max(t.started_at), avg(t.pass_rate), sum(t.fault_sites),"       \
    " avg(t.elapsed_seconds), sum(t.failed)"                                             \
    " FROM suites s LEFT JOIN run_totals t ON t.suite_id = s.id"

enum {
    DAL_SUITE_COL_NAME = 0,
    DAL_SUITE_COL_RUNS,
    DAL_SUITE_COL_CASES,
    DAL_SUITE_COL_FIRST_RUN_AT,
    DAL_SUITE_COL_LAST_RUN_AT,
    DAL_SUITE_COL_PASS_RATE,
    DAL_SUITE_COL_FAULT_SITES,
    DAL_SUITE_COL_ELAPSED_SECONDS,
    DAL_SUITE_COL_FAILURES
};

static FLSuiteRow dal_suite_row(sqlite3_stmt *stmt) {
    FLSuiteRow row = {
        .name            = dal_column_text(stmt, DAL_SUITE_COL_NAME),
        .runs            = sqlite3_column_int64(stmt, DAL_SUITE_COL_RUNS),
        .cases           = sqlite3_column_int64(stmt, DAL_SUITE_COL_CASES),
        .first_run_at    = dal_column_text(stmt, DAL_SUITE_COL_FIRST_RUN_AT),
        .last_run_at     = dal_column_text(stmt, DAL_SUITE_COL_LAST_RUN_AT),
        .pass_rate       = sqlite3_column_double(stmt, DAL_SUITE_COL_PASS_RATE),
        .fault_sites     = sqlite3_column_int64(stmt, DAL_SUITE_COL_FAULT_SITES),
        .elapsed_seconds = sqlite3_column_double(stmt, DAL_SUITE_COL_ELAPSED_SECONDS),
        .failures        = sqlite3_column_int64(stmt, DAL_SUITE_COL_FAILURES),
    };

    return row;
}

void faultline_suite_list(sqlite3 *db, i64 limit, FLSuiteVisitor *visit, void *user) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW

    FL_TRY {
        // A suite without runs has a NULL max(run_id), which sorts last in DESC order.
        stmt = dal_prepare_or_throw(db,
                                    DAL_SUITE_SELECT
                                    " GROUP BY s.id ORDER BY max(t.run_id) DESC, s.name"
                                    " LIMIT :limit;",
                                    faultline_dal_read_failed);
        dal_bind_limit(db, stmt, limit, faultline_dal_read_failed);

        int rc = SQLITE_DONE;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            FLSuiteRow const row = dal_suite_row(stmt);
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

bool faultline_suite_find(sqlite3 *db, char const *name, FLSuiteVisitor *visit,
                          void *user) {
    sqlite3_stmt *volatile stmt  = NULL; // volatile: read in FL_CATCH_ALL_RETHROW
    bool                   found = false;

    FL_TRY {
        stmt = dal_prepare_or_throw(db, DAL_SUITE_SELECT " WHERE s.name = ?1 GROUP BY s.id;",
                                    faultline_dal_read_failed);
        dal_bind_text(db, stmt, 1, name, faultline_dal_read_failed);

        int const rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            FLSuiteRow const row = dal_suite_row(stmt);
            found                = true;
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
