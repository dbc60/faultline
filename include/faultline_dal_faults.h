#ifndef FAULTLINE_DAL_FAULTS_H_
#define FAULTLINE_DAL_FAULTS_H_

/**
 * @file faultline_dal_faults.h
 * @author Douglas Cuthbertson
 * @brief Faults slice of the data-access layer.
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
 * @brief One source location and the faults recorded there.
 */
typedef struct FLHotspotRow {
    char const *source_file;
    int         source_line;
    i64         faults; // faults recorded at the location
    i64         cases;  // distinct cases that recorded one
} FLHotspotRow;

typedef bool FLHotspotVisitor(FLHotspotRow const *row, void *user);

/**
 * @brief Visit source locations, the most faults first. Faults without a source file
 * are left out.
 *
 * @param suite_name only this suite's faults, or NULL for every suite
 * @param limit the most locations to visit; 0 or less visits every location
 * @throw faultline_dal_read_failed
 */
extern void faultline_hotspot_list(sqlite3 *db, char const *suite_name, i64 limit,
                                   FLHotspotVisitor *visit, void *user);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_DAL_FAULTS_H_
