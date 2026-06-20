/**
 * @file faultline_core_unity.c
 * @author Douglas Cuthbertson
 * @brief Unity build of the OS-free application layer -> faultline_core.lib
 * @version 0.1
 * @date 2026-06-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This translation unit is the "application layer" half of the platform/app
 * split. It contains the test driver, the command layer, and result
 * reporting -- everything that acts ON test suites. It must NOT reach the OS
 * directly: it allocates, times, logs, throws, opens files, and loads suite
 * modules only through the services handed to it in an FLPlatformAPI.
 *
 * Build it with NO /DFL_PLATFORM_BUILD (so FL_MALLOC routes through the injected
 * memory service, not flp_malloc) and with an include path that exposes
 * platform_api.h + faultline/* interface headers but NOT the flp_* headers or
 * <windows.h>. If a platform dependency ever creeps back in, this TU stops
 * compiling -- that is the enforced "clean dependency direction".
 *
 * NOTE: several includes below do not exist yet / require the seam refactors
 *       described in the design discussion. See faultline_core.cmd for the
 *       full prerequisite checklist. This file is the target partition.
 */

/* -- Portable foundations --------------------------------------------------- */
#include "fnv/FNV64.c"
#include "set.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "command.c"

/* -- Shared exception reason tokens (one definition for the whole binary) ---- */
#include "fl_exception_service.c"

/* -- Application-side service accessors ------------------------------------
 * These give the core its OWN g_fla_* service globals + fla_set_* setters,
 * exactly like a suite DLL. faultline_app_main() installs the platform's
 * vtables into them at entry. Compiling this TU WITHOUT /DFL_PLATFORM_BUILD makes
 * fl_try.h / FL_MALLOC resolve to these fla_ accessors rather than flp_ direct
 * calls -- that is seam #3, achieved by the build flag alone.
 */
#include "fla_exception_service.c"
#include "fla_memory_service.c"
#include "fla_log_service.c"

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

/* -- Application entry point -----------------------------------------------
 * NEW: extract the portable orchestration currently inside main()'s FL_TRY
 * body (parse_command_with_globals -> configure logging -> init db ->
 * faultline_initialize -> build ExecutionContext -> dispatch) into
 * faultline_app_main(FLPlatformAPI const *, int argc, char **argv). The first
 * thing it does is install the injected services (memory/log/exception/timer/
 * file) into itself, exactly as the platform injects them into suite DLLs.
 */
#include "faultline_app_main.c"
