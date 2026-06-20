/**
 * @file win32_faultline_unity.c
 * @author Douglas Cuthbertson
 * @brief Unity build of the Win32 platform layer -> faultline.exe
 * @version 0.1
 * @date 2026-06-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This translation unit is the "platform layer" half of the platform/app
 * split. It is the ONLY code permitted to touch the OS: memory paging,
 * threads, timers, files, and module loading. It implements the services
 * (exception / logging / fault-injecting memory / timer / file / module),
 * builds an FLPlatformAPI, and calls faultline_app_main() in faultline_core.lib.
 *
 * Build it WITH /DFL_PLATFORM_BUILD and link faultline_core.lib + the prebuilt
 * sqlite3.obj / cwalk.obj. Dependency direction is platform -> core only.
 *
 * NOTE: the flp_timer_service.c / flp_file_service.c / flp_module_service.c
 *       includes are NEW and must be authored (see faultline_split.cmd for the
 *       prerequisite checklist). This file is the target partition.
 */

/* -- Memory infrastructure (OS-backed) -------------------------------------- */
#include "region.c"
#include "region_node.c"
#include "region_os.c" /* dispatches to region_windows.c -> VirtualAlloc */
#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"

/* -- OS primitives ---------------------------------------------------------- */
#include "fl_threads.c"
#include "win_timer.c"

/* -- Platform service implementations --------------------------------------- */
#include "flp_exception_service.c"
#include "flp_log_service.c"
#include "flp_memory_service.c"
#include "flp_fault_memory_service.c"
#include "flp_timer_service.c"  /* NEW: wraps QueryPerformanceCounter   */
#include "flp_file_service.c"   /* NEW: wraps stdio behind FLFileService */
#include "flp_module_service.c" /* NEW: LoadLibrary/GetProcAddress/inject */

/* -- Platform host entry point (thin) -----------------------------------------
 * main() sets up the arena + services, fills an FLPlatformAPI, then calls
 * faultline_app_main(&platform, argc, argv). All OS coupling stops here.
 *
 * NOTE: this is the split-architecture host, NOT the original main_windows.c
 * (which is still used by the monolithic main_unity_windows.c build). The two
 * coexist until the split build replaces the monolith.
 */
#include "win32_faultline_main.c"
