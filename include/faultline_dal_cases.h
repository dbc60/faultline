#ifndef FAULTLINE_DAL_CASES_H_
#define FAULTLINE_DAL_CASES_H_

/**
 * @file faultline_dal_cases.h
 * @author Douglas Cuthbertson
 * @brief Cases slice of the data-access layer: case history, regressions against each
 * case's baseline, and baseline resets.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // i64
#include <sqlite/sqlite3.h>                 // sqlite3

#include <stdbool.h> // bool

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief One case: how often it ran and failed, and its baseline and latest results.
 */
typedef struct FLCaseRow {
    char const *suite_name;
    char const *name;
    i64         appearances;              // runs that recorded the case
    i64         failures;                 // those results worse than FL_PASS
    bool        has_baseline;             // false after the baseline run is deleted
    i64         baseline_fault_sites;     // 0 when has_baseline is false
    double      baseline_elapsed_seconds; // 0 when has_baseline is false
    char const *baseline_set_at;          // NULL when has_baseline is false
    i64         last_fault_sites;         // from the latest run that recorded the case
    double      last_elapsed_seconds;     // from the latest run that recorded the case
} FLCaseRow;

typedef bool FLCaseVisitor(FLCaseRow const *row, void *user);

/**
 * @brief Visit cases, ordered by suite name and then case name.
 *
 * @param suite_name only this suite's cases, or NULL for every suite
 * @param limit the most cases to visit; 0 or less visits every case
 * @throw faultline_dal_read_failed
 */
extern void faultline_case_list(sqlite3 *db, char const *suite_name, i64 limit,
                                FLCaseVisitor *visit, void *user);

/**
 * @brief One case whose latest result regressed against its baseline result.
 */
typedef struct FLRegressionRow {
    char const *suite_name;
    char const *case_name;
    i64         baseline_fault_sites;
    i64         last_fault_sites;
    double      baseline_elapsed_seconds;
    double      last_elapsed_seconds;
    bool        coverage_regression; // last_fault_sites < baseline_fault_sites
    bool        runtime_regression;  // slower than the baseline by more than the threshold
} FLRegressionRow;

typedef bool FLRegressionVisitor(FLRegressionRow const *row, void *user);

/**
 * @brief Visit cases whose latest result exercised fewer fault sites than their
 * baseline result, or ran slower than it by more than threshold_pct percent.
 *
 * Fault-site coverage of an unchanged case is deterministic, so a drop points at the
 * code under test or at fault discovery. Runtime varies from run to run, hence the
 * threshold. Cases without a baseline are left out. The largest relative loss of
 * coverage comes first.
 *
 * @param suite_name only this suite's cases, or NULL for every suite
 * @param limit the most cases to visit; 0 or less visits every case
 * @param threshold_pct runtime growth, in percent, that counts as a regression
 * @throw faultline_dal_read_failed
 */
extern void faultline_regression_list(sqlite3 *db, char const *suite_name, i64 limit,
                                      double threshold_pct, FLRegressionVisitor *visit,
                                      void *user);

/**
 * @brief Make each matching case's latest run its baseline.
 *
 * @param suite_name only this suite's cases, or NULL for every suite
 * @param case_name only cases with this name, or NULL for every case
 * @return the number of cases changed
 * @throw faultline_dal_write_failed
 */
extern i64 faultline_baseline_reset(sqlite3 *db, char const *suite_name,
                                    char const *case_name);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_DAL_CASES_H_
