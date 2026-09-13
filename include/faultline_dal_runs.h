#ifndef FAULTLINE_DAL_RUNS_H_
#define FAULTLINE_DAL_RUNS_H_

/**
 * @file faultline_dal_runs.h
 * @author Douglas Cuthbertson
 * @brief Runs slice of the data-access layer: record a run, and read runs, their
 * results, their failures, and their daily trends.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // i64
#include <faultline/fl_context.h>           // FLContext
#include <faultline/fl_result_codes.h>      // FLResultCode
#include <sqlite/sqlite3.h>                 // sqlite3

#include <stdbool.h> // bool

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Record a finished suite run: its suite, the run, and each case's result and
 * faults, inside one savepoint.
 *
 * A case the database has not seen before is added with this run as its baseline.
 *
 * @param db connection from faultline_init_database
 * @param fctx the context of a suite whose cases have all run
 * @return the id of the new run
 * @throw faultline_dal_write_failed when a write fails, including when two of the
 *        suite's cases share a name. Nothing of the run is recorded.
 */
extern i64 faultline_run_record(sqlite3 *db, FLContext *fctx);

/**
 * @brief One run and its totals.
 */
typedef struct FLRunRow {
    i64         id;
    char const *suite_name;
    char const *started_at;         // UTC, in FL_DB_TIME_FORMAT
    i64         case_count;         // cases the suite defined
    i64         cases_run;          // cases with a recorded result
    i64         passed;             // results equal to FL_PASS
    i64         fault_sites;        // summed over the results
    double      elapsed_seconds;    // summed over the results
    double      pass_rate;          // passed / cases_run, or 0 when no case ran
    i64         discovery_failures; // summed over the results
    i64         injection_failures; // summed over the results
} FLRunRow;

typedef bool FLRunVisitor(FLRunRow const *row, void *user);

/**
 * @brief Visit runs, newest first.
 *
 * @param suite_name only this suite's runs, or NULL for every suite
 * @param limit the most runs to visit; 0 or less visits every run
 * @throw faultline_dal_read_failed
 */
extern void faultline_run_list(sqlite3 *db, char const *suite_name, i64 limit,
                               FLRunVisitor *visit, void *user);

/**
 * @brief Visit one run.
 *
 * @return false, without calling the visitor, when no run has the id
 * @throw faultline_dal_read_failed
 */
extern bool faultline_run_find(sqlite3 *db, i64 run_id, FLRunVisitor *visit, void *user);

/**
 * @brief One case's result in a run, with the location of its first fault.
 */
typedef struct FLResultRow {
    char const  *case_name;
    FLResultCode result_code;
    double       elapsed_seconds;
    i64          fault_sites;
    char const  *reason;      // NULL when none was recorded
    char const  *details;     // NULL when none were recorded
    char const  *source_file; // NULL when the result has no fault with a file
    int          source_line;
} FLResultRow;

typedef bool FLResultVisitor(FLResultRow const *row, void *user);

/**
 * @brief Visit a run's results in case order.
 *
 * @throw faultline_dal_read_failed
 */
extern void faultline_result_list(sqlite3 *db, i64 run_id, FLResultVisitor *visit,
                                  void *user);

/**
 * @brief The runs a failure list reads.
 */
typedef enum FLFailureScope {
    FL_FAILURES_LATEST, // each suite's latest run
    FL_FAILURES_ALL     // every run
} FLFailureScope;

/**
 * @brief One fault of a result worse than FL_PASS. A failed result without faults is
 * one row whose source_file is NULL.
 */
typedef struct FLFailureRow {
    i64          run_id;
    char const  *suite_name;
    char const  *case_name;
    FLResultCode result_code;
    char const  *reason;           // NULL when none was recorded
    char const  *details;          // NULL when none were recorded
    char const  *source_file;      // NULL when there is no fault or it has no file
    int          source_line;
    i64          resource_address; // 0 when the fault names no resource
} FLFailureRow;

typedef bool FLFailureVisitor(FLFailureRow const *row, void *user);

/**
 * @brief Visit failures, newest run first.
 *
 * @param suite_name only this suite's failures, or NULL for every suite
 * @param limit the most rows to visit; 0 or less visits every row
 * @throw faultline_dal_read_failed
 */
extern void faultline_failure_list(sqlite3 *db, char const *suite_name, i64 limit,
                                   FLFailureScope scope, FLFailureVisitor *visit,
                                   void *user);

/**
 * @brief One suite's runs on one day, with the values of the suite's previous day.
 */
typedef struct FLTrendRow {
    char const *suite_name;
    char const *day;                      // YYYY-MM-DD, UTC
    i64         runs;
    double      pass_rate;                // mean over the day's runs, 0..1
    double      elapsed_seconds;          // mean over the day's runs
    i64         failures;                 // results worse than FL_LEAK, summed
    bool        has_previous;             // false on the suite's first day
    double      previous_pass_rate;       // undefined when has_previous is false
    double      previous_elapsed_seconds; // undefined when has_previous is false
    i64         previous_failures;        // undefined when has_previous is false
} FLTrendRow;

typedef bool FLTrendVisitor(FLTrendRow const *row, void *user);

/**
 * @brief Visit days with runs: suites in name order, each suite's days newest first.
 *
 * @param suite_name only this suite's days, or NULL for every suite
 * @param limit the most rows to visit; 0 or less visits every row
 * @throw faultline_dal_read_failed
 */
extern void faultline_trend_list(sqlite3 *db, char const *suite_name, i64 limit,
                                 FLTrendVisitor *visit, void *user);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_DAL_RUNS_H_
