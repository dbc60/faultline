@ECHO OFF

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: project.cmd - Repository-specific settings for the MSVC build. options.cmd, shell.cmd,
:: setup.cmd and config.cmd read these.
::
:: setup.cmd calls this before config.cmd so the settings are in setup.cmd's environment.
:: The DIR_* paths setup.cmd derives are already set. Reassign DIR_INCLUDE, DIR_SRC,
:: DIR_THIRD_PARTY, DIR_METRICS or DIR_TEST here to change the repository layout.
SET DIR_FL=%DIR_REPO%

:: FaultLine source files, grouped by which side of the platform/core split they
:: belong to (see fl_macros.h's FL_DECL_SPEC), for a build script that compiles them as
:: separate translation units rather than #including them into a unity build. List only
:: the sources this repository has. DIR_FL is the root of the FaultLine tree.
::
:: FL_PLATFORM_SOURCES: the flp_* platform-side implementations plus the core arena/
:: region allocator they depend on. Only ever belongs to the platform driver (the .exe).
:: FL_CORE_SOURCES: the fla_* core-side accessors plus the shared FLExceptionReason
:: constants. Belongs to whichever binary uses the core libraries: the platform driver's
:: own embedded bootstrap (built with /DFL_EMBEDDED) and the application DLL (built with
:: /DDLL_BUILD).
::
:: fl_exception.c is in the core list, not the platform list, because exceptions are not
:: a service: every image compiles that one file exactly once and calls fl_push/fl_pop/
:: fl_throw directly. Each module getting its own copy of the reason strings is by
:: design, because FaultLine treats a reason as an address token, so FL_CATCH matches by
:: pointer within a module and cross-module catches must use FL_CATCH_STR (see fl_try.h).
SET FL_PLATFORM_SOURCES=%DIR_FL%\src\flp_log_service.c ^
    %DIR_FL%\src\flp_stream_service.c %DIR_FL%\src\flp_timer_service.c ^
    %DIR_FL%\src\flp_file_service.c %DIR_FL%\src\flp_memory_service.c ^
    %DIR_FL%\src\arena.c ^
    %DIR_FL%\src\arena_malloc.c %DIR_FL%\src\arena_dbg.c %DIR_FL%\src\arena_pool.c ^
    %DIR_FL%\src\digital_search_tree.c %DIR_FL%\src\region.c ^
    %DIR_FL%\src\region_node.c %DIR_FL%\src\region_os.c %DIR_FL%\src\lock_os.c
:: fla_timer_service.c: a suite's fl_run_case times the test body through the timer
:: service, so a suite that omits it does not link.
SET FL_CORE_SOURCES=%DIR_FL%\src\fla_log_service.c ^
    %DIR_FL%\src\fla_memory_service.c %DIR_FL%\src\fla_file_service.c ^
    %DIR_FL%\src\fla_timer_service.c %DIR_FL%\src\fl_exception.c

:: Include paths and compiler flags build_test_dll.cmd adds to config.cmd's flags
SET PROJECT_INCLUDES=/I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% /I%DIR_SRC%
SET PROJECT_COMPILER_FLAGS=

:: The test driver that runs a suite, and the script under build\cmd\ that builds it
:: when it is missing. Leave the script empty for a prebuilt driver.
SET FL_TEST_DRIVER=%DIR_OUT_BIN%\faultline.exe
SET FL_TEST_DRIVER_SCRIPT=faultline.cmd

if %trace% EQU 1 (
    ECHO FL_PLATFORM_SOURCES = %FL_PLATFORM_SOURCES%
    ECHO FL_CORE_SOURCES = %FL_CORE_SOURCES%
)
