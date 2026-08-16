@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the test framework package into dist\test_framework\.
::
:: The package ships the test-declaration header a suite DLL compiles against:
:: the FL_TEST / FL_SUITE_* / FL_GET_TEST_SUITE macro family and the
:: FLTestCase / FLTestSuite structures the driver enumerates through the
:: fl_get_test_suite export. The same macro emits the build-identity export a
:: host reads before it trusts a module, so the package also ships fl_abi.h and
:: the fl_threads.h it takes its C11 threads types from. It is header-only and
:: declares a dependency on exception_service, which supplies the fl_try.h /
:: fl_macros.h closure the headers include and the exception sources a suite
:: links against. Import exception_service first; fl_import enforces the order.
::
:: Usage:
::   build\cmd\test_framework_dist.cmd [clean]
::
::   clean   - remove dist\test_framework\ entirely
::
:: Output layout:
::   dist\test_framework\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Test Framework Distribution
TITLE %PROJECT_NAME%

:: Derive DIR_REPO from this script's location (build\cmd\test_framework_dist.cmd)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%

SET DIR_DIST=%DIR_REPO%\dist\test_framework
SET DIR_INC=%DIR_REPO%\include\faultline

:: Package metadata recorded in manifest.txt (bump SVC_VERSION on release).
SET SVC_NAME=test_framework
SET SVC_VERSION=0.2.0
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
IF EXIST "%DIR_DIST%\include" RD /S /Q "%DIR_DIST%\include"
MD "%DIR_DIST%\include\faultline"

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\
:: -----------------------------------------------------------------------
ECHO Copying public headers...
COPY /Y "%DIR_INC%\fl_test.h"                "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_abi.h"                 "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_threads.h"             "%DIR_DIST%\include\faultline\" > NUL

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
ECHO Done. Package written to dist\test_framework\
ECHO.
ECHO   Depends on            : exception_service (import it first)
ECHO   Consumer include path : dist\test_framework\include
ECHO   Compile these sources : none (header-only)

GOTO :SUCCESS

:FAIL
ENDLOCAL
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
