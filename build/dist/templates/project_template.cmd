@ECHO OFF

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: project.cmd - Repository-specific settings for the MSVC build scripts from
:: FaultLine's build_scripts package. Copy this file to project.cmd and edit it.
:: fl_import never writes project.cmd, so the copy survives package updates.
::
:: setup.cmd calls this after it sets DIR_REPO, DIR_INCLUDE, DIR_SRC, DIR_THIRD_PARTY,
:: DIR_METRICS, DIR_TEST and the DIR_OUT_* output paths, and before config.cmd.
:: Reassign the layout ones first if this repository differs, e.g.
::   SET DIR_SRC=%DIR_REPO%\tests

:: Root of the imported FaultLine tree, the -Into given to fl_import
SET DIR_FL=%DIR_THIRD_PARTY%\faultline

:: FaultLine sources build_test_dll.cmd links for --fl-core. List only imported ones:
:: these two come from the exceptions and timer_service packages. Add
:: %DIR_FL%\src\fla_memory_service.c when memory_service is imported.
SET FL_PLATFORM_SOURCES=
SET FL_CORE_SOURCES=%DIR_FL%\src\fla_timer_service.c %DIR_FL%\src\fl_exception.c

:: Include paths and compiler flags build_test_dll.cmd adds to config.cmd's flags
SET PROJECT_INCLUDES=/I%DIR_FL%\include /I%DIR_FL%\src /I%DIR_INCLUDE% /I%DIR_SRC%
SET PROJECT_COMPILER_FLAGS=

:: The prebuilt FaultLine driver that runs each suite. It must come from the same
:: toolchain as the suites, or it skips them as incompatible.
SET FL_TEST_DRIVER=C:\path\to\faultline.exe
SET FL_TEST_DRIVER_SCRIPT=

:: Each suite's build script is one line, e.g. build\cmd\my_suite.cmd:
::   @ECHO OFF
::   CALL "%~dp0build_test_dll.cmd" "My Suite" "my_suite" "my_suite_tests.c" "my_suite_tests" "" --fl-core %*
