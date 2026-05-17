@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the driver-side exception package into dist\exception-driver\.
::
:: This package is for test drivers that load test-suite DLLs. It provides:
::   - flp_exception_service.c  (driver TLS stack — push/pop/throw)
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
:: same binary — both define fl_throw_assertion and will cause a link error.
::
:: Usage:
::   build\cmd\exception_driver_dist.cmd [clean]
::
::   clean   - remove dist\exception-driver\ entirely
::
:: Output layout:
::   dist\exception-driver\src\      - C sources (compile selectively — see above)
::   dist\exception-driver\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Driver Exception Distribution
TITLE %PROJECT_NAME%

:: Derive DIR_REPO from this script's location (build\cmd\exception_driver_dist.cmd)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%

SET DIR_DIST=%DIR_REPO%\dist\exception-driver
SET DIR_SRC=%DIR_REPO%\src
SET DIR_INC=%DIR_REPO%\include\faultline

:: Handle clean
IF /I "%~1"=="clean" (
    IF EXIST "%DIR_DIST%" (
        ECHO Removing %DIR_DIST%
        RD /S /Q "%DIR_DIST%"
    ) ELSE (
        ECHO Nothing to clean.
    )
    GOTO :SUCCESS
)

:: Create output directories
IF NOT EXIST "%DIR_DIST%\src"                  MD "%DIR_DIST%\src"
IF NOT EXIST "%DIR_DIST%\include\faultline"    MD "%DIR_DIST%\include\faultline"

:: -----------------------------------------------------------------------
:: C source files
:: -----------------------------------------------------------------------
ECHO Copying source files...
COPY /Y "%DIR_SRC%\fl_exception_service.c"   "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\fla_exception_service.c"  "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_exception_service.c"  "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\
:: -----------------------------------------------------------------------
ECHO Copying public headers...
COPY /Y "%DIR_INC%\fl_abbreviated_types.h"          "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fla_exception_service.h"         "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_service.h"          "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_service_assert.h"   "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_types.h"            "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_macros.h"                     "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_try.h"                        "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_REPO%\include\flp_exception_service.h" "%DIR_DIST%\include\" > NUL

ECHO.
ECHO Done. Package written to dist\exception-driver\
ECHO.
ECHO   Consumer include path : dist\exception-driver\include
ECHO   Driver compiles       : fl_exception_service.c  flp_exception_service.c
ECHO   DLL compiles          : fl_exception_service.c  fla_exception_service.c

GOTO :SUCCESS

:SUCCESS
ENDLOCAL
EXIT /B 0
