@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Set" "set" "set_tests.c" "set_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
