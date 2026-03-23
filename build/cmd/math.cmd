@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Math" "math" "math_tests.c" "math_tests" "" %*
EXIT /B %ERRORLEVEL%
