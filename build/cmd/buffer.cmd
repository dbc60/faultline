@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Buffer Module" "buffer" "buffer_tests.c" "buffer_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
