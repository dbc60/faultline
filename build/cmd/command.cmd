@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Command" "command" "command_tests.c" "command_tests" "" %*
EXIT /B %ERRORLEVEL%
