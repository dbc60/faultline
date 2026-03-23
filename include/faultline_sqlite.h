#ifndef FAULTLINE_SQLITE_H_
#define FAULTLINE_SQLITE_H_

/**
 * @file faultline_sqlite.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.2.0
 * @date 2025-09-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_context.h>         // for FLContext
#include <sqlite/sqlite3.h>               // for sqlite3
#include <stdbool.h>                      // for bool
#include <time.h>                         // for time_t
#include <faultline/fl_test_summary.h>    // for FLTestSummary
#include <faultline/fl_exception_types.h> // for FLExceptionReason

#if defined(__cplusplus)
extern "C" {
#endif

extern FLExceptionReason faultline_db_create_failed;
extern FLExceptionReason faultline_db_not_found;

// Database connection management
extern sqlite3 *faultline_init_database(char const *db_path);
extern void     faultline_close_database(sqlite3 *db);

// Test run recording functions
extern int  faultline_record_test_run_start(sqlite3 *db, char const *suite_name,
                                            time_t start_time);
extern void faultline_record_test_run_complete(sqlite3 *db, int run_id, FLContext *fctx);
extern void faultline_record_test_summary(sqlite3 *db, int run_id,
                                          FLTestSummary *summary, char const *test_name);

// Database query and reporting functions
extern void faultline_show_recent_runs(sqlite3 *db, int limit);
extern void faultline_show_test_failures(sqlite3 *db, char const *suite_name, int limit,
                                         bool show_all_history);
extern void faultline_show_run_details(sqlite3 *db, int run_id);
extern void faultline_show_suite_summary(sqlite3 *db, char const *suite_name);

// Main API functions from Stage 6 requirements
extern void faultline_export_sqlite(FLContext *fctx, char const *db_path);
extern void faultline_import_sqlite(FLContext *fctx, char const *db_path, int run_id);

// Schema management
extern void faultline_sqlite_init_schema(char const *db_path);
extern void faultline_sqlite_migrate_schema(char const *db_path, int current_version);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_SQLITE_H_
