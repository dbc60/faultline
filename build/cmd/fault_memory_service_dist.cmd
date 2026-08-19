@ECHO OFF
SETLOCAL

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the standalone platform memory service package into dist\fault_memory_service\.
::
:: This package contains everything a new project's platform executable needs
:: to use the FaultLine memory service: the arena allocator, fault injector,
:: platform- and application-side service implementations, and all public and
:: private headers required to compile them.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\fault_memory_service_dist.cmd [clean]
::
::   clean   - remove dist\fault_memory_service\ entirely
::
:: Output layout:
::   dist\fault_memory_service\src\      - C sources and private headers (compile these)
::   dist\fault_memory_service\src\fnv\  - FNV hash library (compile FNV64.c; add src\ to include path)
::   dist\fault_memory_service\include\  - public headers (add to consumer include path)
::
:: Consumer build setup:
::   Include paths : dist\fault_memory_service\include
::                   dist\fault_memory_service\src       (private headers + fnv\)
::   Compile       : dist\fault_memory_service\src\*.c
::                   dist\fault_memory_service\src\fnv\FNV64.c

SET PROJECT_NAME=Standalone Fault-Injecting Memory Service Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO FAULT_MEMORY_SERVICE_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package fault_memory_service %FL_CLEAN%
EXIT /B %ERRORLEVEL%
