@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Build every service distribution package into dist\.
::
:: Each package is produced by its own build\cmd\<name>_dist.cmd, which wipes
:: the package directory, copies the package's sources and headers, and
:: generates its manifest. This script runs them all and stops at the first
:: failure, so a package that could not be collected never passes for a
:: complete one.
::
:: Usage:
::   build\cmd\all_dist.cmd
::   build\cmd\all_dist.cmd clean
::
::   clean   - remove dist\ entirely. cleanall and cleanplat do the same: a
::             package is a plain file collection with no per-configuration or
::             per-platform output to distinguish.
::
:: Output layout:
::   dist\<name>\  - one directory per package, each with its own manifest.txt

SET PROJECT_NAME=Service Distribution Packages
TITLE %PROJECT_NAME%

:: Derive DIR_REPO from this script's location (build\cmd\)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%
SET DIR_DIST=%DIR_REPO%\dist

:: The packages to build. Each name maps to build\cmd\<name>_dist.cmd. Order is
:: not significant: every script reads from the repository, never from another
:: package's output. Add a package by adding its name here.
SET "DIST_PACKAGES=exception_service fault_memory_service file_service log_service memory_service stream_service test_framework timer_service"

:: Handle clean. The per-package scripts each remove only their own dist\<name>\;
:: removing dist\ outright also sweeps up packages that no longer have a script.
IF /I "%~1"=="clean"     GOTO :CLEAN
IF /I "%~1"=="cleanall"  GOTO :CLEAN
IF /I "%~1"=="cleanplat" GOTO :CLEAN
IF NOT "%~1"=="" (
    ECHO ALL_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    GOTO :FAIL
)

FOR %%P IN (%DIST_PACKAGES%) DO (
    CALL "%DIR_CMD%\%%P_dist.cmd"
    IF ERRORLEVEL 1 (
        ECHO ALL_DIST.CMD ERROR: %%P_dist.cmd failed -- dist\ is incomplete. 1>&2
        GOTO :FAIL
    )
)

:: Each package script retitles the console; put our own title back.
TITLE %PROJECT_NAME%
ECHO.
ECHO Done. All packages written to dist\
FOR %%P IN (%DIST_PACKAGES%) DO ECHO   dist\%%P\
GOTO :SUCCESS

:CLEAN
IF EXIST "%DIR_DIST%" (
    ECHO Removing %DIR_DIST%
    RD /S /Q "%DIR_DIST%"
    IF EXIST "%DIR_DIST%" (
        ECHO ALL_DIST.CMD ERROR: could not remove %DIR_DIST% -- a file in it is 1>&2
        ECHO open, or a shell is sitting in the tree. 1>&2
        GOTO :FAIL
    )
) ELSE (
    ECHO Nothing to clean.
)
GOTO :SUCCESS

:FAIL
ENDLOCAL
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
