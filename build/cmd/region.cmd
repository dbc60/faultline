@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Region" "region" "region_tests.c" "region_tests" "" %*
EXIT /B %ERRORLEVEL%
