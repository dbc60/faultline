@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Stream Service" "fl_stream_service" "flp_stream_service_tests.c" "flp_stream_service_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
