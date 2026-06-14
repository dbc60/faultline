/**
 * @file faultline_tests.c
 * @author Douglas Cuthbertson
 * @brief "Faultline Driver" test-suite registration.
 * @version 0.1
 * @date 2025-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This translation unit contains only the suite registration. The embedded test
 * cases it references are defined in faultline_test.c and faultline_sqlite_test.c
 * and forward-declared via faultline_tests_cases.h, so this file builds either as
 * a standalone TU (standard build) or #included into faultline_tests_unity.c
 * (unity build).
 */
#include <faultline/fault.h>
#include <faultline/fl_test.h>
#include "faultline_tests_cases.h" // extern decls for the embedded test cases

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(load_driver_test)
FL_SUITE_ADD_EMBEDDED(begin_end_test)
FL_SUITE_ADD_EMBEDDED(is_valid_test)
FL_SUITE_ADD_EMBEDDED(next_index_test)
FL_SUITE_ADD_EMBEDDED(case_name_test)
FL_SUITE_ADD_EMBEDDED(index_test)
FL_SUITE_ADD_EMBEDDED(test_driver)
FL_SUITE_ADD_EMBEDDED(results_test)
FL_SUITE_ADD_EMBEDDED(discovery_failure_recording)
FL_SUITE_ADD_EMBEDDED(injection_failure_recording)
FL_SUITE_ADD_EMBEDDED(result_validation)
FL_SUITE_ADD_EMBEDDED(schema_creates_database)
FL_SUITE_ADD_EMBEDDED(schema_creates_core_tables)
FL_SUITE_ADD_EMBEDDED(schema_sets_version)
FL_SUITE_ADD_EMBEDDED(schema_creates_foreign_keys)
FL_SUITE_ADD_EMBEDDED(schema_creates_analysis_tables)
FL_SUITE_ADD_EMBEDDED(schema_creates_views)
FL_SUITE_ADD_EMBEDDED(schema_counts_successes)
FL_SUITE_ADD_EMBEDDED(schema_tracks_baseline)
FL_SUITE_ADD_EMBEDDED(schema_reports_regressions)
FL_SUITE_ADD_EMBEDDED(schema_reports_trends)
FL_SUITE_ADD_EMBEDDED(schema_migrates_v1)
FL_SUITE_ADD_EMBEDDED(schema_invalid_path_throws)
FL_SUITE_ADD_EMBEDDED(schema_permission_denied_throws)
FL_SUITE_ADD_EMBEDDED(schema_existing_database_succeeds)
FL_SUITE_END;

FL_GET_TEST_SUITE("Faultline Driver", ts)
