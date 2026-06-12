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

typedef struct {
    FLTestCase  tc;
    char const *test_db;
} TestSchema;

#endif // FAULTLINE_SQLITE_TEST_H_
