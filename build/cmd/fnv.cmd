@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "FNV" "fnv" "fnv_tests.c" "fnv_tests" "" %*
EXIT /B %ERRORLEVEL%
