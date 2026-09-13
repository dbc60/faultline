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
FL_SUITE_ADD_EMBEDDED(schema_stamps_fresh_database)
FL_SUITE_ADD_EMBEDDED(schema_reopen_is_idempotent)
FL_SUITE_ADD_EMBEDDED(schema_creates_every_table)
FL_SUITE_ADD_EMBEDDED(schema_creates_every_index)
FL_SUITE_ADD_EMBEDDED(schema_creates_every_view)
FL_SUITE_ADD_EMBEDDED(schema_enforces_foreign_keys)
FL_SUITE_ADD_EMBEDDED(schema_checks_match_enums)
FL_SUITE_ADD_EMBEDDED(schema_runs_in_wal)
FL_SUITE_ADD_EMBEDDED(schema_refuses_future_version)
FL_SUITE_ADD_EMBEDDED(schema_refuses_legacy_schema)
FL_SUITE_ADD_EMBEDDED(schema_failure_rolls_back)
FL_SUITE_ADD_EMBEDDED(schema_invalid_path_throws)
FL_SUITE_ADD_EMBEDDED(schema_read_only_throws)
FL_SUITE_ADD_EMBEDDED(dal_record_round_trips)
FL_SUITE_ADD_EMBEDDED(dal_record_refuses_duplicates)
FL_SUITE_ADD_EMBEDDED(dal_run_totals_match)
FL_SUITE_ADD_EMBEDDED(dal_baseline_first_sighting)
FL_SUITE_ADD_EMBEDDED(dal_run_delete_cascades)
FL_SUITE_ADD_EMBEDDED(dal_run_list_newest_first)
FL_SUITE_ADD_EMBEDDED(dal_run_find_results)
FL_SUITE_ADD_EMBEDDED(dal_failures_latest_by_id)
FL_SUITE_ADD_EMBEDDED(dal_suite_list_find)
FL_SUITE_ADD_EMBEDDED(dal_case_history)
FL_SUITE_ADD_EMBEDDED(dal_regressions_classified)
FL_SUITE_ADD_EMBEDDED(dal_baseline_reset)
FL_SUITE_ADD_EMBEDDED(dal_trends_by_day)
FL_SUITE_ADD_EMBEDDED(dal_hotspots_ranked)
FL_SUITE_END;

FL_GET_TEST_SUITE("Faultline Driver", ts)
