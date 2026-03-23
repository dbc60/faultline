@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Timer" "timer" "timer_tests.c" "timer_tests" "" %*
EXIT /B %ERRORLEVEL%
