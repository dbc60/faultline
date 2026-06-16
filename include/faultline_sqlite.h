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
extern void faultline_show_suites(sqlite3 *db, int limit, bool verbose);
extern void faultline_show_cases(sqlite3 *db, char const *suite_name, int limit,
                                 bool verbose);
extern void faultline_show_regressions(sqlite3 *db, char const *suite_name, int limit,
                                       double threshold_pct);

/**
 * @brief One test's coverage/runtime regression versus its baseline.
 *
 * The string fields point into the query's storage and are only valid for the
 * duration of the FLRegressionFn callback; copy them if needed afterwards.
 */
typedef struct FLRegression {
    char const *suite_name;
    char const *test_name;
    long long   baseline_fault_sites;
    long long   last_fault_sites;
    double      baseline_execution_time;
    double      last_execution_time;
    bool        coverage_regression; // last_fault_sites < baseline_fault_sites
    bool        runtime_regression;  // last_execution_time over baseline by threshold
} FLRegression;

typedef void (*FLRegressionFn)(FLRegression const *regression, void *ctx);

extern int faultline_for_each_regression(sqlite3 *db, char const *suite_name, int limit,
                                         double threshold_pct, FLRegressionFn fn,
                                         void *ctx);

extern int faultline_reset_baselines(sqlite3 *db, char const *suite_name,
                                     char const *test_name);

extern void faultline_show_trends(sqlite3 *db, char const *suite_name, int limit);

/**
 * @brief One suite's aggregated health for a single day, with the prior day's
 * values for trend direction.
 *
 * has_prev is false for a suite's earliest day, where the prev_* fields are
 * undefined. The string fields point into the query's storage and are only
 * valid for the duration of the FLSuiteTrendFn callback.
 */
typedef struct FLSuiteTrend {
    char const *suite_name;
    char const *day; // 'YYYY-MM-DD'
    int         total_runs;
    double      avg_pass_rate;      // 0..1
    double      avg_execution_time; // seconds
    long long   total_failures;
    bool        has_prev;
    double      prev_pass_rate;
    double      prev_execution_time;
    long long   prev_total_failures;
} FLSuiteTrend;

typedef void (*FLSuiteTrendFn)(FLSuiteTrend const *trend, void *ctx);

extern int faultline_for_each_trend(sqlite3 *db, char const *suite_name, int limit,
                                    FLSuiteTrendFn fn, void *ctx);

extern void faultline_show_hotspots(sqlite3 *db, char const *suite_name, int limit);

/**
 * @brief A source location and how many faults it produced.
 *
 * The string field points into the query's storage and is only valid for the
 * duration of the FLFaultHotspotFn callback.
 */
typedef struct FLFaultHotspot {
    char const *source_file;
    int         source_line;
    long long   failure_count;  // total faults recorded at this location
    long long   tests_affected; // distinct test cases that hit it
} FLFaultHotspot;

typedef void (*FLFaultHotspotFn)(FLFaultHotspot const *hotspot, void *ctx);

extern int faultline_for_each_hotspot(sqlite3 *db, char const *suite_name, int limit,
                                      FLFaultHotspotFn fn, void *ctx);

// Main API functions from Stage 6 requirements
extern void faultline_export_sqlite(FLContext *fctx, char const *db_path);
extern void faultline_import_sqlite(FLContext *fctx, char const *db_path, int run_id);

// Schema management
#define FL_SCHEMA_VERSION 2
extern void faultline_sqlite_init_schema(char const *db_path);
extern void faultline_sqlite_migrate_schema(char const *db_path, int current_version);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_SQLITE_H_
