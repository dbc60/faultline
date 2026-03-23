@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Bits" "bits" "bits_tests.c" "bits_tests" "" %*
EXIT /B %ERRORLEVEL%
