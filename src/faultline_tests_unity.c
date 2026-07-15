/**
 * @file faultline_tests_unity.c
 * @author Douglas Cuthbertson
 * @brief Unity build of the "Faultline Driver" test suite: pulls all
 *        implementation and test-case source into one translation unit, then
 *        the suite registration (faultline_tests.c).
 * @version 0.1
 * @date 2026-06-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
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
#include "fl_threads.c" // mtx_init, mtx_lock, mtx_unlock, mtx_destroy
#include "fla_exception_service.c"
#include "fla_log_service.c"
#include "fla_timer_service.c" // g_fla_timer_service for the driver's injected timing
#include "fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"
#include "set.c"
#include "win_timer.c"

#include "faultline_tests.c"
