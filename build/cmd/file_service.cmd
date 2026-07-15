@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "File Service" "fl_file_service" "flp_file_service_tests.c" "flp_file_service_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
