@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
::
:: The suite ends in an AddressSanitizer report on arena memory by design. all.cmd
:: builds it only with asan and runs it through asan_reports.cmd canary, never with the
:: other suites.
CALL "%~dp0build_test_dll.cmd" "ASan Arena Canary" "asan_arena_canary" "asan_arena_canary_tests.c" "asan_arena_canary_tests" "" %*
EXIT /B %ERRORLEVEL%
