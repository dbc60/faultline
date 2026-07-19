@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Arena Pool" "arena_pool" "arena_pool_tests.c" "arena_pool_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
