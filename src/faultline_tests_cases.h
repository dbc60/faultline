#ifndef FAULTLINE_TESTS_CASES_H_
#define FAULTLINE_TESTS_CASES_H_

/**
 * @file faultline_tests_cases.h
 * @author Douglas Cuthbertson
 * @brief Forward declarations of the embedded test-case objects that make up the
 *        "Faultline Driver" suite, so the suite registration can live in its own
 *        translation unit (non-unity / standard build).
 * @version 0.1
 * @date 2026-06-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_test.h>     // FL_TYPE_TEST_DECL
#include "faultline_test.h"        // TestDriverData
#include "faultline_sqlite_test.h" // TestSchema

// Driver tests (src/faultline_test.c)
FL_TYPE_TEST_DECL(TestDriverData, load_driver_test);
FL_TYPE_TEST_DECL(TestDriverData, begin_end_test);
FL_TYPE_TEST_DECL(TestDriverData, is_valid_test);
FL_TYPE_TEST_DECL(TestDriverData, next_index_test);
FL_TYPE_TEST_DECL(TestDriverData, case_name_test);
FL_TYPE_TEST_DECL(TestDriverData, index_test);
FL_TYPE_TEST_DECL(TestDriverData, test_driver);
FL_TYPE_TEST_DECL(TestDriverData, results_test);
FL_TYPE_TEST_DECL(TestDriverData, discovery_failure_recording);
FL_TYPE_TEST_DECL(TestDriverData, injection_failure_recording);
FL_TYPE_TEST_DECL(TestDriverData, result_validation);

// SQLite schema tests (src/faultline_sqlite_test.c)
FL_TYPE_TEST_DECL(TestSchema, schema_creates_database);
FL_TYPE_TEST_DECL(TestSchema, schema_creates_core_tables);
FL_TYPE_TEST_DECL(TestSchema, schema_sets_version);
FL_TYPE_TEST_DECL(TestSchema, schema_creates_foreign_keys);
FL_TYPE_TEST_DECL(TestSchema, schema_creates_analysis_tables);
FL_TYPE_TEST_DECL(TestSchema, schema_creates_views);
FL_TYPE_TEST_DECL(TestSchema, schema_tracks_baseline);
FL_TYPE_TEST_DECL(TestSchema, schema_reports_regressions);
FL_TYPE_TEST_DECL(TestSchema, schema_reports_trends);
FL_TYPE_TEST_DECL(TestSchema, schema_migrates_v1);
FL_TYPE_TEST_DECL(TestSchema, schema_invalid_path_throws);
FL_TYPE_TEST_DECL(TestSchema, schema_permission_denied_throws);
FL_TYPE_TEST_DECL(TestSchema, schema_existing_database_succeeds);

#endif // FAULTLINE_TESTS_CASES_H_
