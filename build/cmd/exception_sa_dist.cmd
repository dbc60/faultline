@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the standalone exception package into dist\exception\.
::
:: Usage:
::   build\cmd\exception_sa_dist.cmd [clean]
::
::   clean   - remove dist\exception\ entirely
::
:: Output layout:
::   dist\exception\src\      - C sources (compile these)
::   dist\exception\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Standalone Exception Distribution
TITLE %PROJECT_NAME%

:: Derive DIR_REPO from this script's location (build\cmd\exception_sa_dist.cmd)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%

SET DIR_DIST=%DIR_REPO%\dist\exception
SET DIR_SRC=%DIR_REPO%\src
SET DIR_INC=%DIR_REPO%\include\faultline
SET DIR_SA=%DIR_REPO%\standalone\exception

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

:: -----------------------------------------------------------------------
:: Standalone-specific header (stub that strips FL_TEST to plain functions)
:: -----------------------------------------------------------------------
ECHO Copying standalone-specific headers...
COPY /Y "%DIR_SA%\include\faultline\fl_test.h"      "%DIR_DIST%\include\faultline\" > NUL

ECHO.
ECHO Done. Package written to dist\exception\
ECHO.
ECHO   Consumer include path : dist\exception\include
ECHO   Compile these sources : dist\exception\src\*.c

GOTO :SUCCESS

:SUCCESS
ENDLOCAL
EXIT /B 0
