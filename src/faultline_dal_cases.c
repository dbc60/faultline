/**
 * @file faultline_dal_cases.c
 * @author Douglas Cuthbertson
 * @brief Cases slice of the data-access layer: case history, regressions against each
 * case's baseline, and baseline resets.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit. #included by faultline_dal.c after its statement
 * helpers.
 *
 * A case's baseline result is its result in cases.baseline_run_id. Its latest result is
 * the one from the largest run id that recorded it.
 */

enum {
    DAL_CASE_COL_SUITE_NAME = 0,
    DAL_CASE_COL_NAME,
    DAL_CASE_COL_APPEARANCES,
    DAL_CASE_COL_FAILURES,
    DAL_CASE_COL_HAS_BASELINE,
    DAL_CASE_COL_BASELINE_FAULT_SITES,
    DAL_CASE_COL_BASELINE_ELAPSED_SECONDS,
    DAL_CASE_COL_BASELINE_SET_AT,
    DAL_CASE_COL_LAST_FAULT_SITES,
    DAL_CASE_COL_LAST_ELAPSED_SECONDS
};

void faultline_case_list(sqlite3 *db, char const *suite_name, i64 limit,
                         FLCaseVisitor *visit, void *user) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW

    FL_TRY {
        // result_code > 1 counts results worse than FL_PASS.
        stmt = dal_prepare_or_throw(
            db,
            "SELECT s.name, c.name,"
            " (SELECT count(*) FROM results x WHERE x.case_id = c.id),"
            " (SELECT count(*) FROM results x WHERE x.case_id = c.id AND x.result_code > 1),"
            " b.id IS NOT NULL, b.fault_sites, b.elapsed_seconds,"
            " CASE WHEN b.id IS NULL THEN NULL ELSE c.baseline_set_at END,"
            " l.fault_sites, l.elapsed_seconds"
            " FROM cases c"
            " JOIN suites s ON s.id = c.suite_id"
            " LEFT JOIN results b ON b.case_id = c.id AND b.run_id = c.baseline_run_id"
            " LEFT JOIN results l ON l.case_id = c.id"
            "   AND l.run_id = (SELECT max(x.run_id) FROM results x WHERE x.case_id = c.id)"
            " WHERE (?1 IS NULL OR s.name = ?1)"
            " ORDER BY s.name, c.name"
            " LIMIT :limit;",
            faultline_dal_read_failed);
        dal_bind_text(db, stmt, 1, suite_name, faultline_dal_read_failed);
        dal_bind_limit(db, stmt, limit, faultline_dal_read_failed);

        int rc = SQLITE_DONE;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            FLCaseRow const row = {
                .suite_name   = dal_column_text(stmt, DAL_CASE_COL_SUITE_NAME),
                .name         = dal_column_text(stmt, DAL_CASE_COL_NAME),
                .appearances  = sqlite3_column_int64(stmt, DAL_CASE_COL_APPEARANCES),
                .failures     = sqlite3_column_int64(stmt, DAL_CASE_COL_FAILURES),
                .has_baseline = sqlite3_column_int(stmt, DAL_CASE_COL_HAS_BASELINE) != 0,
                .baseline_fault_sites
                = sqlite3_column_int64(stmt, DAL_CASE_COL_BASELINE_FAULT_SITES),
                .baseline_elapsed_seconds
                = sqlite3_column_double(stmt, DAL_CASE_COL_BASELINE_ELAPSED_SECONDS),
                .baseline_set_at  = dal_column_text(stmt, DAL_CASE_COL_BASELINE_SET_AT),
                .last_fault_sites = sqlite3_column_int64(stmt, DAL_CASE_COL_LAST_FAULT_SITES),
                .last_elapsed_seconds
                = sqlite3_column_double(stmt, DAL_CASE_COL_LAST_ELAPSED_SECONDS),
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

enum {
    DAL_REGRESSION_COL_SUITE_NAME = 0,
    DAL_REGRESSION_COL_CASE_NAME,
    DAL_REGRESSION_COL_BASELINE_FAULT_SITES,
    DAL_REGRESSION_COL_LAST_FAULT_SITES,
    DAL_REGRESSION_COL_BASELINE_ELAPSED_SECONDS,
    DAL_REGRESSION_COL_LAST_ELAPSED_SECONDS
};

void faultline_regression_list(sqlite3 *db, char const *suite_name, i64 limit,
                               double threshold_pct, FLRegressionVisitor *visit,
                               void *user) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW

    // A run slower than its baseline by threshold_pct percent takes this many times the
    // baseline's seconds.
    double const slowdown = 1.0 + threshold_pct / 100.0;

    FL_TRY {
        // The inner joins leave out cases without a baseline result. max(baseline, 1)
        // keeps a baseline of 0 fault sites from dividing by zero.
        stmt = dal_prepare_or_throw(
            db,
            "SELECT suite_name, case_name, baseline_sites, last_sites, baseline_seconds,"
            " last_seconds"
            " FROM ("
            "   SELECT s.name AS suite_name, c.name AS case_name,"
            "          b.fault_sites AS baseline_sites, b.elapsed_seconds AS baseline_seconds,"
            "          l.fault_sites AS last_sites, l.elapsed_seconds AS last_seconds"
            "   FROM cases c"
            "   JOIN suites s ON s.id = c.suite_id"
            "   JOIN results b ON b.case_id = c.id AND b.run_id = c.baseline_run_id"
            "   JOIN results l ON l.case_id = c.id"
            "     AND l.run_id = (SELECT max(x.run_id) FROM results x WHERE x.case_id = c.id)"
            "   WHERE (?1 IS NULL OR s.name = ?1)"
            " )"
            " WHERE last_sites < baseline_sites"
            "    OR (baseline_seconds > 0 AND last_seconds > baseline_seconds * ?2)"
            " ORDER BY CAST(baseline_sites - last_sites AS REAL) / max(baseline_sites, 1) DESC,"
            "          suite_name, case_name"
            " LIMIT :limit;",
            faultline_dal_read_failed);
        dal_bind_text(db, stmt, 1, suite_name, faultline_dal_read_failed);
        dal_bind_double(db, stmt, 2, slowdown, faultline_dal_read_failed);
        dal_bind_limit(db, stmt, limit, faultline_dal_read_failed);

        int rc = SQLITE_DONE;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            FLRegressionRow row = {
                .suite_name = dal_column_text(stmt, DAL_REGRESSION_COL_SUITE_NAME),
                .case_name  = dal_column_text(stmt, DAL_REGRESSION_COL_CASE_NAME),
                .baseline_fault_sites
                = sqlite3_column_int64(stmt, DAL_REGRESSION_COL_BASELINE_FAULT_SITES),
                .last_fault_sites
                = sqlite3_column_int64(stmt, DAL_REGRESSION_COL_LAST_FAULT_SITES),
                .baseline_elapsed_seconds
                = sqlite3_column_double(stmt, DAL_REGRESSION_COL_BASELINE_ELAPSED_SECONDS),
                .last_elapsed_seconds
                = sqlite3_column_double(stmt, DAL_REGRESSION_COL_LAST_ELAPSED_SECONDS),
            };

            // The same two conditions the query selected on, to say which one held.
            row.coverage_regression = row.last_fault_sites < row.baseline_fault_sites;
            row.runtime_regression
                = row.baseline_elapsed_seconds > 0.0
                  && row.last_elapsed_seconds > row.baseline_elapsed_seconds * slowdown;

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

i64 faultline_baseline_reset(sqlite3 *db, char const *suite_name, char const *case_name) {
    sqlite3_stmt *volatile stmt    = NULL; // volatile: read in FL_CATCH_ALL_RETHROW
    i64                    changed = 0;

    // One table, so no savepoint: a single statement is already atomic.
    FL_TRY {
        stmt = dal_prepare_or_throw(
            db,
            "UPDATE cases"
            " SET baseline_run_id"
            "       = (SELECT max(x.run_id) FROM results x WHERE x.case_id = cases.id),"
            "     baseline_set_at = strftime('" FL_DB_TIME_FORMAT "', 'now')"
            " WHERE (?1 IS NULL OR suite_id = (SELECT id FROM suites WHERE name = ?1))"
            "   AND (?2 IS NULL OR name = ?2);",
            faultline_dal_write_failed);
        dal_bind_text(db, stmt, 1, suite_name, faultline_dal_write_failed);
        dal_bind_text(db, stmt, 2, case_name, faultline_dal_write_failed);
        dal_step_done_or_throw(db, stmt, faultline_dal_write_failed);
        changed = sqlite3_changes(db);
    }
    FL_CATCH_ALL_RETHROW {
        sqlite3_finalize(stmt);
    }
    FL_END_TRY;

    sqlite3_finalize(stmt);
    return changed;
}
