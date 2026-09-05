@ECHO OFF
SETLOCAL

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the timer service package into dist\timer_service\.
::
:: The package ships only the timer's own files and declares a dependency on
:: exceptions, which supplies the fl_try.h / assert header closure the
:: provider compiles against and the exception implementations it links
:: against. Import exceptions first; fl_import enforces the order.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\timer_service_dist.cmd [clean]
::
::   clean   - remove dist\timer_service\ entirely
::
:: Output layout:
::   dist\timer_service\src\      - C sources (compile these)
::   dist\timer_service\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Timer Service Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO TIMER_SERVICE_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package timer_service %FL_CLEAN%
EXIT /B %ERRORLEVEL%
