@ECHO OFF
SETLOCAL

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the file service package into dist\file_service\.
::
:: The package ships only the file service's own files and declares a
:: dependency on memory_service, which supplies the FL_MALLOC selector and
:: implementation the provider allocates its UTF-16 path buffers through, plus
:: the exception/log closure it compiles against. Import memory_service first;
:: fl_import enforces the order.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\file_service_dist.cmd [clean]
::
::   clean   - remove dist\file_service\ entirely
::
:: Output layout:
::   dist\file_service\src\      - C sources (compile these)
::   dist\file_service\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=File Service Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO FILE_SERVICE_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package file_service %FL_CLEAN%
EXIT /B %ERRORLEVEL%
