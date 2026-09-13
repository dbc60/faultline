/**
 * @file faultline_dal_faults.c
 * @author Douglas Cuthbertson
 * @brief Faults slice of the data-access layer.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Not a standalone translation unit. #included by faultline_dal.c after its statement
 * helpers.
 */

enum {
    DAL_HOTSPOT_COL_SOURCE_FILE = 0,
    DAL_HOTSPOT_COL_SOURCE_LINE,
    DAL_HOTSPOT_COL_FAULTS,
    DAL_HOTSPOT_COL_CASES
};

void faultline_hotspot_list(sqlite3 *db, char const *suite_name, i64 limit,
                            FLHotspotVisitor *visit, void *user) {
    sqlite3_stmt *volatile stmt = NULL; // volatile: read in FL_CATCH_ALL_RETHROW

    FL_TRY {
        stmt = dal_prepare_or_throw(
            db,
            "SELECT f.source_file, f.source_line, count(*) AS faults,"
            " count(DISTINCT x.case_id)"
            " FROM faults f"
            " JOIN results x ON x.id = f.result_id"
            " JOIN runs r ON r.id = x.run_id"
            " JOIN suites s ON s.id = r.suite_id"
            " WHERE (?1 IS NULL OR s.name = ?1) AND f.source_file IS NOT NULL"
            " GROUP BY f.source_file, f.source_line"
            " ORDER BY faults DESC, f.source_file, f.source_line"
            " LIMIT :limit;",
            faultline_dal_read_failed);
        dal_bind_text(db, stmt, 1, suite_name, faultline_dal_read_failed);
        dal_bind_limit(db, stmt, limit, faultline_dal_read_failed);

        int rc = SQLITE_DONE;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            FLHotspotRow const row = {
                .source_file = dal_column_text(stmt, DAL_HOTSPOT_COL_SOURCE_FILE),
                .source_line = sqlite3_column_int(stmt, DAL_HOTSPOT_COL_SOURCE_LINE),
                .faults      = sqlite3_column_int64(stmt, DAL_HOTSPOT_COL_FAULTS),
                .cases       = sqlite3_column_int64(stmt, DAL_HOTSPOT_COL_CASES),
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
