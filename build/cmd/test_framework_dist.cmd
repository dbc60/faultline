@ECHO OFF
SETLOCAL

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the test framework package into dist\test_framework\.
::
:: The package ships the test-declaration header a suite DLL compiles against:
:: the FL_TEST / FL_SUITE_* / FL_GET_TEST_SUITE macro family and the
:: FLTestCase / FLTestSuite structures the driver enumerates through the
:: fl_get_test_suite export. The same macro emits the build-identity export a
:: host reads before it trusts a module, so the package also ships fl_abi.h and
:: the fl_threads.h it takes its C11 threads types from. It is header-only and
:: declares a dependency on exceptions, which supplies the fl_try.h /
:: fl_macros.h closure the headers include and the exception sources a suite
:: links against. Import exceptions first; fl_import enforces the order.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\test_framework_dist.cmd [clean]
::
::   clean   - remove dist\test_framework\ entirely
::
:: Output layout:
::   dist\test_framework\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Test Framework Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO TEST_FRAMEWORK_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package test_framework %FL_CLEAN%
EXIT /B %ERRORLEVEL%
