@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Fault Injector" "fault_injector" "fault_injector_tests.c" "fault_injector_tests" "/wd4456" %*
EXIT /B %ERRORLEVEL%
