@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
::
:: The suite ends in an AddressSanitizer report by design. all.cmd builds it only
:: with asan and runs it through asan_reports.cmd canary, never with the other suites.
CALL "%~dp0build_test_dll.cmd" "ASan Canary" "asan_canary" "asan_canary_tests.c" "asan_canary_tests" "" %*
EXIT /B %ERRORLEVEL%
