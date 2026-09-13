#ifndef FAULTLINE_SQLITE_TEST_H_
#define FAULTLINE_SQLITE_TEST_H_

/**
 * @file faultline_sqlite_test.h
 * @author Douglas Cuthbertson
 * @brief Test-case type for the SQLite schema tests.
 * @version 0.1
 * @date 2025-09-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_test.h> // FLTestCase
#include <sqlite/sqlite3.h>    // sqlite3

typedef struct {
    FLTestCase tc;
    sqlite3   *db;       // closed by cleanup if a case leaves one open
    char       path[64]; // this case's private database file
} TestSchema;

#endif // FAULTLINE_SQLITE_TEST_H_
