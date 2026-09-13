#ifndef FAULTLINE_DAL_H_
#define FAULTLINE_DAL_H_

/**
 * @file faultline_dal.h
 * @author Douglas Cuthbertson
 * @brief Data-access layer over the results database.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Every function here follows two rules:
 *
 * - Reads allocate nothing. A read passes each row to a visitor, one call per row, and
 *   stops when the visitor returns false. A row's char const * fields point into SQLite's
 *   statement and stop being valid when the visitor returns, so a visitor that keeps a
 *   string copies its bytes.
 * - Failures throw faultline_dal_read_failed or faultline_dal_write_failed. A write that
 *   throws leaves nothing it started in the database. "Not found" is not a failure, and
 *   returns false.
 */
#include <faultline/fl_exception_types.h> // FLExceptionReason

#include <faultline_dal_cases.h>
#include <faultline_dal_faults.h>
#include <faultline_dal_runs.h>
#include <faultline_dal_suites.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern FLExceptionReason faultline_dal_read_failed;
extern FLExceptionReason faultline_dal_write_failed;

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_DAL_H_
