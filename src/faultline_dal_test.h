#ifndef FAULTLINE_DAL_TEST_H_
#define FAULTLINE_DAL_TEST_H_

/**
 * @file faultline_dal_test.h
 * @author Douglas Cuthbertson
 * @brief Test-case type for the data-access layer tests.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/arena.h>   // Arena
#include <faultline/fl_test.h> // FLTestCase
#include <sqlite/sqlite3.h>    // sqlite3

typedef struct {
    FLTestCase tc;
    sqlite3   *db;       // closed by cleanup if a case leaves one open
    Arena     *arena;    // backs the FLContext a case records
    char       path[64]; // this case's private database file
} TestDal;

#endif // FAULTLINE_DAL_TEST_H_
