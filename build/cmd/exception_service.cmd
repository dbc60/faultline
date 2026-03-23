@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Exception Service" "fl_exception" "fl_exception_tests.c" "fl_exception_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
