@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Each suite ends in an AddressSanitizer report on arena memory by design. all.cmd
:: builds them only with asan and runs each through asan_reports.cmd canary, never with
:: the other suites. ASan ends the process at its first report, so each case in
:: asan_arena_canary_tests.c, chosen by ASAN_ARENA_CANARY, is built into a DLL of its
:: own.
SET "_CANARY_ARGS=%*"

CALL :BUILD free_middle FREE_MIDDLE
IF ERRORLEVEL 1 EXIT /B 1
CALL :BUILD free_links FREE_LINKS
IF ERRORLEVEL 1 EXIT /B 1
CALL :BUILD free_footer FREE_FOOTER
IF ERRORLEVEL 1 EXIT /B 1
CALL :BUILD overflow OVERFLOW
IF ERRORLEVEL 1 EXIT /B 1
CALL :BUILD underflow UNDERFLOW
EXIT /B %ERRORLEVEL%

:: :BUILD <case> <CASE> builds asan_arena_canary_<case>_tests.dll with
:: ASAN_ARENA_CANARY=CANARY_<CASE>.
:BUILD
CALL "%~dp0build_test_dll.cmd" "ASan Arena Canary" "asan_arena_canary" ^
    "asan_arena_canary_tests.c" "asan_arena_canary_%~1_tests" ^
    "/DASAN_ARENA_CANARY=CANARY_%~2" %_CANARY_ARGS%
EXIT /B %ERRORLEVEL%
