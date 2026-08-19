@ECHO OFF
SETLOCAL
:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Thin wrapper around build\dist\fl_dist.ps1 for the standalone (fault-free)
:: memory service package.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: This package deliberately OMITS fault injection. For the fault-injecting
:: variant, use fault_memory_service_dist.cmd instead.
::
:: Usage:
::   build\cmd\memory_service_dist.cmd [clean]
::
::   clean   - remove dist\memory_service\ entirely
::
:: Output layout:
::   dist\memory_service\src\      - C sources and private headers (compile these)
::   dist\memory_service\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Standalone Memory Service Distribution (no fault injection)
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO MEMORY_SERVICE_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package memory_service %FL_CLEAN%
EXIT /B %ERRORLEVEL%
