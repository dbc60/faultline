@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
:: Always C++. This suite is not dual-dialect source, so cxx is forced on
:: rather than offered: passing it here overrides whatever the caller chose.
CALL "%~dp0build_test_dll.cmd" "C++ Suite" "cxx_suite" "cxx_suite_tests.cpp" "cxx_suite_tests" "" cxx %*
EXIT /B %ERRORLEVEL%
