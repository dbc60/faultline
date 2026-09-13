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
#include "faultline_dal_test.h"    // TestDal

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
FL_TYPE_TEST_DECL(TestSchema, schema_stamps_fresh_database);
FL_TYPE_TEST_DECL(TestSchema, schema_reopen_is_idempotent);
FL_TYPE_TEST_DECL(TestSchema, schema_creates_every_table);
FL_TYPE_TEST_DECL(TestSchema, schema_creates_every_index);
FL_TYPE_TEST_DECL(TestSchema, schema_creates_every_view);
FL_TYPE_TEST_DECL(TestSchema, schema_enforces_foreign_keys);
FL_TYPE_TEST_DECL(TestSchema, schema_checks_match_enums);
FL_TYPE_TEST_DECL(TestSchema, schema_runs_in_wal);
FL_TYPE_TEST_DECL(TestSchema, schema_refuses_future_version);
FL_TYPE_TEST_DECL(TestSchema, schema_refuses_legacy_schema);
FL_TYPE_TEST_DECL(TestSchema, schema_failure_rolls_back);
FL_TYPE_TEST_DECL(TestSchema, schema_invalid_path_throws);
FL_TYPE_TEST_DECL(TestSchema, schema_read_only_throws);

// Data-access layer tests (src/faultline_dal_test.c)
FL_TYPE_TEST_DECL(TestDal, dal_record_round_trips);
FL_TYPE_TEST_DECL(TestDal, dal_record_refuses_duplicates);
FL_TYPE_TEST_DECL(TestDal, dal_run_totals_match);
FL_TYPE_TEST_DECL(TestDal, dal_baseline_first_sighting);
FL_TYPE_TEST_DECL(TestDal, dal_run_delete_cascades);
FL_TYPE_TEST_DECL(TestDal, dal_run_list_newest_first);
FL_TYPE_TEST_DECL(TestDal, dal_run_find_results);
FL_TYPE_TEST_DECL(TestDal, dal_failures_latest_by_id);
FL_TYPE_TEST_DECL(TestDal, dal_suite_list_find);
FL_TYPE_TEST_DECL(TestDal, dal_case_history);
FL_TYPE_TEST_DECL(TestDal, dal_regressions_classified);
FL_TYPE_TEST_DECL(TestDal, dal_baseline_reset);
FL_TYPE_TEST_DECL(TestDal, dal_trends_by_day);
FL_TYPE_TEST_DECL(TestDal, dal_hotspots_ranked);

#endif // FAULTLINE_TESTS_CASES_H_
