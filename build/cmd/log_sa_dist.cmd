@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the standalone log service package into dist\log\.
::
:: Usage:
::   build\cmd\log_sa_dist.cmd [clean]
::
::   clean   - remove dist\log\ entirely
::
:: Output layout:
::   dist\log\src\      - C sources (compile these)
::   dist\log\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Standalone Log Service Distribution
TITLE %PROJECT_NAME%

:: Derive DIR_REPO from this script's location (build\cmd\log_sa_dist.cmd)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%

SET DIR_DIST=%DIR_REPO%\dist\log
SET DIR_SRC=%DIR_REPO%\src
SET DIR_INC=%DIR_REPO%\include\faultline

:: Package metadata recorded in manifest.txt (bump SVC_VERSION on release).
SET SVC_NAME=log
SET SVC_VERSION=0.2.0
SET SVC_DEPENDS=

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

:: Wipe any previously-collected files so the package reflects exactly the
:: current file set and the generated manifest matches what is on disk.
IF EXIST "%DIR_DIST%\src"     RD /S /Q "%DIR_DIST%\src"
IF EXIST "%DIR_DIST%\include" RD /S /Q "%DIR_DIST%\include"
MD "%DIR_DIST%\src"
MD "%DIR_DIST%\include\faultline"

:: -----------------------------------------------------------------------
:: C source files
:: -----------------------------------------------------------------------
ECHO Copying source files...
COPY /Y "%DIR_SRC%\fl_threads.c"             "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_log_service.c"        "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\fla_log_service.c"        "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\
:: -----------------------------------------------------------------------
ECHO Copying public headers...
COPY /Y "%DIR_INC%\fla_log_service.h"        "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_log.h"                 "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_log_types.h"           "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_macros.h"              "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_threads.h"               "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_try.h"                   "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_service_assert.h" "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_REPO%\include\flp_log_service.h" "%DIR_DIST%\include\" > NUL

:: -----------------------------------------------------------------------
:: Generate the package manifest (authoritative file list used by fl_import)
:: -----------------------------------------------------------------------
ECHO Generating manifest...
powershell -NoProfile -ExecutionPolicy Bypass -File "%DIR_BUILD%\dist\fl_emit_manifest.ps1" -PackageDir "%DIR_DIST%" -Name "%SVC_NAME%" -Version "%SVC_VERSION%" -Depends "%SVC_DEPENDS%"
IF ERRORLEVEL 1 (
    ECHO ERROR: manifest generation failed.
    GOTO :FAIL
)

ECHO.
ECHO Done. Package written to dist\log\
ECHO.
ECHO   Consumer include path : dist\log\include
ECHO   Compile these sources : dist\log\src\*.c

GOTO :SUCCESS

:FAIL
ENDLOCAL
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
