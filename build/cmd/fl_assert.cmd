@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Assert Macros" "fl_assert" "fl_assert_tests.c" "fl_assert_tests" "" %*
EXIT /B %ERRORLEVEL%
