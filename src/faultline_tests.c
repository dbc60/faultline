/**
 * @file faultline_butts.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "fault_injector.c"
#include "fla_memory_service.c"
#include "faultline_context.c"
#include "faultline_driver.c"
#include "faultline_sqlite.c"
#include "faultline_sqlite_test.c"
#include "faultline_test.c"
#include "fl_exception_service.c"
#include "fla_exception_service.c"
#include "fla_log_service.c"
#include "../third_party/fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "set.c"
#include "win_timer.c"
#include <faultline/fault.h>
#include <faultline/fl_test.h>

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
FL_SUITE_ADD_EMBEDDED(schema_invalid_path_throws)
FL_SUITE_ADD_EMBEDDED(schema_permission_denied_throws)
FL_SUITE_ADD_EMBEDDED(schema_existing_database_succeeds)
FL_SUITE_END;

FL_GET_TEST_SUITE("Faultline Driver", ts)
