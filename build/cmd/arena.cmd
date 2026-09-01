@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Arena" "arena" "arena_tests.c" "arena_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
