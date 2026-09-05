/**
 * @file faultline_core_unity.c
 * @author Douglas Cuthbertson
 * @brief Unity build of the OS-free application layer -> faultline_core.lib
 * @version 0.1
 * @date 2026-06-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This translation unit is the "application layer" half of the platform/app split. It
 * contains the test driver, the command layer, and result reporting -- everything that
 * acts ON test suites. It must not call OS-specific functions directly: it times, logs,
 * throws, opens files, and loads suite modules only through the services handed to it
 * in an FLPlatformAPI, and it allocates from the portable arena stack compiled below,
 * whose one OS-specific piece (the region paging provider) the platform reprovides.
 *
 * Build it with NO /DFL_PLATFORM_BUILD (so FL_MALLOC routes through the injected
 * memory service, not flp_malloc) and with /DFL_EMBEDDED (the fla_ accessors are
 * built into the executable rather than exported from a DLL). If a platform
 * dependency ever creeps back in, this TU stops compiling -- that is the
 * enforced "clean dependency direction".
 */

/* -- Portable memory stack -------------------------------------------------------------
 * The arena and its region backing are portable core code; the only OS-specific
 * piece is the paging provider (region_os.c -> VirtualAlloc et al.), which the
 * platform TU compiles and the linker resolves into region.c's calls here. A port
 * reprovides region_<os>.c and keeps this stack unchanged.
 */
#include "region.c"
#include "region_node.c"
#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"

/* -- Portable foundations --------------------------------------------------- */
#include "fnv/FNV64.c"
#include "set.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "command.c"

/* -- Shared exception reason tokens (one definition for the whole binary) ---- */
#include "fl_exception.c"

/* -- Application-side service accessors ------------------------------------------------
 * These give the core its OWN g_fla_* service globals + fla_set_* setters, exactly like
 * a suite DLL. faultline_app_main() installs the platform's vtables into them at entry.
 * Compiling this TU WITHOUT /DFL_PLATFORM_BUILD makes fl_try.h / FL_MALLOC resolve to
 * these fla_ accessors rather than flp_ direct calls -- that is seam #3, achieved by the
 * build flag alone.
 */
#include "fla_memory_service.c"
#include "fla_log_service.c"
#include "fla_timer_service.c"
#include "fla_file_service.c"
#include "fla_stream_service.c"

/* -- Test driver ------------------------------------------------------------ */
#include "fault_injector.c"
#include "faultline_context.c"
#include "faultline_driver.c"

/* -- Persistence & reporting ------------------------------------------------ */
#include "faultline_sqlite.c"
#include "output_junit.c"

/* -- Command layer (option/command tables + handlers) ----------------------- */
#include "faultline_commands.c"
#include "command_run.c"
#include "command_show.c"
#include "command_baseline.c"
#include "command_help.c"

/* -- Application entry point -----------------------------------------------------------
 * faultline_app_main(FLPlatformAPI const *, int argc, char **argv): installs the
 * platform's services into this module's fla_ globals, then parses the command line and
 * dispatches -- the same injection a suite DLL receives.
 */
#include "faultline_app_main.c"
