@ECHO OFF
SETLOCAL

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the stream service package into dist\stream_service\.
::
:: The package ships only the stream service's own files (plus fl_file_types.h,
:: whose FLFile type it reuses -- the same shared-vocabulary header the file
:: service package bundles) and declares a dependency on memory_service, which
:: supplies the FL_MALLOC selector and implementation the provider allocates its
:: UTF-16 long-path buffers through, plus the exception/log closure it compiles
:: against. Import memory_service first; fl_import enforces the order.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\stream_service_dist.cmd [clean]
::
::   clean   - remove dist\stream_service\ entirely
::
:: Output layout:
::   dist\stream_service\src\      - C sources (compile these)
::   dist\stream_service\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Stream Service Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO STREAM_SERVICE_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package stream_service %FL_CLEAN%
EXIT /B %ERRORLEVEL%
