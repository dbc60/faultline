@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.
:: /wd4702: cases here throw unconditionally inside FL_TRY, so the fall-through
:: epilogue FL_CATCH and FL_END_TRY emit is unreachable. The back end reports it
:: during LTCG code generation, past the point a warning pragma can reach.
CALL "%~dp0build_test_dll.cmd" "Buffer Module" "buffer" "buffer_tests.c" "buffer_tests" "/wd4456 /wd4702" %*
EXIT /B %ERRORLEVEL%
