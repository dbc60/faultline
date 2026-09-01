@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Memory Service" "fl_memory_service" "flp_memory_service_tests.c" "flp_memory_service_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
