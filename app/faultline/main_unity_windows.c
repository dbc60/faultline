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
#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "buffer.c"
#include "command.c"
#include "digital_search_tree.c"
#include "fault_injector.c"
#include "faultline_context.c"
#include "faultline_driver.c"
#include "faultline_sqlite.c"
#include "fl_exception.c"
#include "fl_threads.c"
#include "flp_log_service.c"
#include "flp_memory_service.c"
#include "flp_timer_service.c"
#include "flp_fault_memory_service.c"
#include "flp_file_service.c"
#include "flp_stream_service.c"
#include "output_junit.c"
#include "fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"
#include "lock_os.c"
#include "set.c"
#include "win_timer.c"

// Command infrastructure
#include "faultline_commands.c"
#include "command_run.c"
#include "command_show.c"
#include "command_baseline.c"
#include "command_help.c"

#include "main_windows.c"
