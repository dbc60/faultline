#ifndef FAULTLINE_DAL_SUITES_H_
#define FAULTLINE_DAL_SUITES_H_

/**
 * @file faultline_dal_suites.h
 * @author Douglas Cuthbertson
 * @brief Suites slice of the data-access layer.
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
 * @brief One suite and the totals of its runs.
 */
typedef struct FLSuiteRow {
    char const *name;
    i64         runs;
    i64         cases;           // cases the suite has recorded
    char const *first_run_at;    // NULL when the suite has no run
    char const *last_run_at;     // NULL when the suite has no run
    double      pass_rate;       // mean over the runs, 0..1
    i64         fault_sites;     // summed over the runs
    double      elapsed_seconds; // mean over the runs
    i64         failures;        // results worse than FL_LEAK, summed over the runs
} FLSuiteRow;

typedef bool FLSuiteVisitor(FLSuiteRow const *row, void *user);

/**
 * @brief Visit suites, the most recently run first. Suites without runs come last.
 *
 * @param limit the most suites to visit; 0 or less visits every suite
 * @throw faultline_dal_read_failed
 */
extern void faultline_suite_list(sqlite3 *db, i64 limit, FLSuiteVisitor *visit,
                                 void *user);

/**
 * @brief Visit one suite.
 *
 * @return false, without calling the visitor, when no suite has the name
 * @throw faultline_dal_read_failed
 */
extern bool faultline_suite_find(sqlite3 *db, char const *name, FLSuiteVisitor *visit,
                                 void *user);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_DAL_SUITES_H_
