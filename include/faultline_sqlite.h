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
#include <sqlite/sqlite3.h>               // for sqlite3
#include <faultline/fl_exception_types.h> // for FLExceptionReason

#if defined(__cplusplus)
extern "C" {
#endif

extern FLExceptionReason faultline_db_create_failed;
extern FLExceptionReason faultline_db_version_too_new;
extern FLExceptionReason faultline_db_legacy_schema;

// The schema version this build creates and opens, stored in PRAGMA user_version.
#define FL_SCHEMA_VERSION 1

// strftime format of every time the database stores: UTC, one-second resolution.
#define FL_DB_TIME_FORMAT "%Y-%m-%dT%H:%M:%SZ"

// Database connection management
extern sqlite3 *faultline_init_database(char const *db_path);
extern void     faultline_close_database(sqlite3 *db);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_SQLITE_H_
