@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Log Service" "fl_log_service" "fl_log_service_tests.c" "fl_log_service_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
