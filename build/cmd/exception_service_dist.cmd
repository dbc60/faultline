@ECHO OFF
SETLOCAL

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the driver-side exception package into dist\exception_service\.
::
:: This package is for test drivers that load test-suite DLLs. It provides:
::   - flp_exception_service.c  (driver TLS stack: push/pop/throw)
::   - fla_exception_service.c  (abort stubs compiled into each DLL so the
::                                driver can inject its service via
::                                fla_set_exception_service)
::   - fl_exception_service.c   (shared exception reason constants)
::
:: To use this package in a test DLL, compile fla_exception_service.c into the
:: DLL and call fla_set_exception_service() after loading it. The driver
:: compiles flp_exception_service.c and calls flp_init_exception_service() to
:: wire the two together.
::
:: DO NOT compile flp_exception_service.c and fla_exception_service.c into the
:: same binary, because both define fl_throw_assertion and will cause a link error.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\exception_service_dist.cmd [clean]
::
::   clean   - remove dist\exception_service\ entirely
::
:: Output layout:
::   dist\exception_service\src\      - C sources (compile selectively - see above)
::   dist\exception_service\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Exception Service Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO EXCEPTION_SERVICE_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package exception_service %FL_CLEAN%
EXIT /B %ERRORLEVEL%
