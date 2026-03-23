@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "DList" "dlist" "dlist_tests.c" "dlist_tests" "" %*
EXIT /B %ERRORLEVEL%
