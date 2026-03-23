@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
CALL "%~dp0build_test_dll.cmd" "Chunk" "chunk" "chunk_tests.c" "chunk_tests" "" %*
EXIT /B %ERRORLEVEL%
