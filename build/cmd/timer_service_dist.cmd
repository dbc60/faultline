@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the timer service package into dist\timer_service\.
::
:: The package ships only the timer's own files and declares a dependency on
:: exception_service, which supplies the fl_try.h / assert header closure the
:: provider compiles against and the exception implementations it links
:: against. Import exception_service first; fl_import enforces the order.
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

:: Derive DIR_REPO from this script's location (build\cmd\timer_service_dist.cmd)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%

SET DIR_DIST=%DIR_REPO%\dist\timer_service
SET DIR_SRC=%DIR_REPO%\src
SET DIR_INC=%DIR_REPO%\include\faultline

:: Package metadata recorded in manifest.txt (bump SVC_VERSION on release).
SET SVC_NAME=timer_service
SET SVC_VERSION=0.1.0
SET SVC_DEPENDS=exception_service

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
COPY /Y "%DIR_SRC%\flp_timer_service.c"      "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\fla_timer_service.c"      "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\
:: -----------------------------------------------------------------------
ECHO Copying public headers...
COPY /Y "%DIR_INC%\fl_timer_service.h"       "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fla_timer_service.h"      "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_timer.h"               "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_stopwatch.h"           "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_REPO%\include\flp_timer_service.h" "%DIR_DIST%\include\" > NUL

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
ECHO Done. Package written to dist\timer_service\
ECHO.
ECHO   Depends on            : exception_service (import it first)
ECHO   Consumer include path : dist\timer_service\include
ECHO   Compile these sources : dist\timer_service\src\*.c

GOTO :SUCCESS

:FAIL
ENDLOCAL
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
