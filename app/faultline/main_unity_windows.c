/**
 * @file main_unity_windows.c
 * @author Douglas Cuthbertson
 * @brief Unity build: pull all implementation source into this translation unit.
 * @version 0.1
 * @date 2026-05-30
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "../../src/arena.c"
#include "../../src/arena_dbg.c"
#include "../../src/arena_malloc.c"
#include "../../src/buffer.c"
#include "../../src/command.c"
#include "../../src/digital_search_tree.c"
#include "../../src/fault_injector.c"
#include "../../src/faultline_context.c"
#include "../../src/faultline_driver.c"
#include "../../src/faultline_sqlite.c"
#include "../../src/fl_exception_service.c"
#include "../../src/fl_threads.c"
#include "../../src/flp_exception_service.c"
#include "../../src/flp_log_service.c"
#include "../../src/flp_memory_service.c"
#include "../../src/flp_fault_memory_service.c"
#include "../../src/output_junit.c"
#include "../../third_party/fnv/FNV64.c"
#include "../../src/region.c"
#include "../../src/region_node.c"
#include "../../src/region_os.c"
#include "../../src/set.c"
#include "../../src/win_timer.c"

// Command infrastructure
#include "faultline_commands.c"
#include "command_run.c"
#include "command_show.c"
#include "command_baseline.c"
#include "command_help.c"

#include "main_windows.c"
