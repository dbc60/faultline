/**
 * @file win32_faultline_unity.c
 * @author Douglas Cuthbertson
 * @brief Unity build of the Win32 platform layer -> the split host executable
 * @version 0.1
 * @date 2026-06-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This translation unit is the "platform layer" half of the platform/app split. It is
 * the only code permitted to touch the OS: memory paging, threads, timers, files, and
 * module loading. It implements the services (exception / logging / fault-injecting
 * memory / timer / file / module), builds an FLPlatformAPI, and calls
 * faultline_app_main() in faultline_core.lib.
 *
 * Build it WITH /DFL_PLATFORM_BUILD and link faultline_core.lib + the prebuilt
 * sqlite3.obj / cwalk.obj. Dependency direction is platform -> core only.
 */

/* -- OS paging provider ---------------------------------------------------------------
 * The portable memory stack (region.c / arena.c / ...) lives in faultline_core.lib;
 * only the paging provider it dispatches to is OS-specific and belongs here. A port
 * reprovides region_<os>.c and keeps the rest.
 */
#include "region_os.c" /* dispatches to region_windows.c -> VirtualAlloc */
#include "lock_os.c"

/* -- OS primitives ---------------------------------------------------------- */
#include "fl_threads.c"
#include "win_timer.c"

/* -- Platform service implementations --------------------------------------- */
/* faultline_core.lib links fla_exception_service.c, whose fl_throw_assertion routes
 * asserts through the injected service; omit this side's copy. */
#define FLP_OMIT_FL_THROW_ASSERTION
#include "flp_exception_service.c"
#include "flp_log_service.c"
#include "flp_memory_service.c"
#include "flp_fault_memory_service.c"
#include "flp_timer_service.c"  /* wraps QueryPerformanceCounter          */
#include "flp_file_service.c"   /* wraps CreateFileW/ReadFile/WriteFile   */
#include "flp_module_service.c" /* LoadLibrary/GetProcAddress + injection */

/* -- Platform host entry point (thin) -----------------------------------------
 * main() sets up the arena + services, fills an FLPlatformAPI, then calls
 * faultline_app_main(&platform, argc, argv). All OS coupling stops here.
 *
 * NOTE: this is the split-architecture host, NOT the original main_windows.c (which is
 * still used by the monolithic main_unity_windows.c build). The two coexist until the
 * split build replaces the monolith.
 */
#include "win32_faultline_main.c"
