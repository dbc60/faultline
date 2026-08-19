@ECHO OFF
SETLOCAL
:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Thin wrapper around build\dist\fl_dist.ps1 for the log service package.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\log_service_dist.cmd [clean]
::
::   clean   - remove dist\log_service\ entirely
::
:: Output layout:
::   dist\log_service\src\      - C sources (compile these)
::   dist\log_service\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Log Service Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO LOG_SERVICE_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package log_service %FL_CLEAN%
EXIT /B %ERRORLEVEL%
