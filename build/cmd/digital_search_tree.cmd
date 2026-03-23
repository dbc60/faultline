@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Digital Search Tree" "digital_search_tree" "digital_search_tree_tests.c" "digital_search_tree_tests" "" %*
EXIT /B %ERRORLEVEL%
