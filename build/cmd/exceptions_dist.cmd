@ECHO OFF
SETLOCAL

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the exception module into dist\exceptions\.
::
:: Exceptions are not a service. The package ships one implementation,
:: fl_exception.c, which every image compiles exactly once: it carries the shared
:: reason constants, fl_push/fl_pop/fl_throw and fl_throw_assertion.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\exceptions_dist.cmd [clean]
::
::   clean   - remove dist\exceptions\ entirely
::
:: Output layout:
::   dist\exceptions\src\      - C sources
::   dist\exceptions\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Exception Module Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO EXCEPTIONS_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package exceptions %FL_CLEAN%
EXIT /B %ERRORLEVEL%
