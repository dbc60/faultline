@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: End-to-end self-test for the FaultLine service distribution pipeline.
::
:: For each package it: (1) runs the dist script to produce dist\<pkg>\,
:: (2) imports the package into an isolated scratch tree via fl_import.ps1,
:: (3) compiles a consumer test against the *imported* tree in /DFL_EMBEDDED
:: mode, and (4) runs it. A failure at any stage is reported and counted; the
:: harness still attempts the remaining packages.
::
:: This recovers the coverage the deleted standalone tests provided (the
:: application-side log service and the single-binary memory-service assembly)
:: and, as a bonus, validates that each package's carved-out file set actually
:: compiles and runs on its own through the real import path.
::
:: Usage:
::   build\dist\selftest\fl_dist_selftest.cmd [build options...]
::   build\dist\selftest\fl_dist_selftest.cmd clean
::
:: Build options (debug/release, x64/x86, vs2022, verbose, ...) are forwarded to
:: options.cmd / setup.cmd, matching the rest of the build system.

SET HERE=%~dp0
SET HERE=%HERE:~0,-1%

:: Set up the same compiler flags and Visual Studio environment the normal build
:: uses. setup.cmd exports DIR_REPO, CommonCompilerFlagsFinal,
:: CommonLinkerFlagsFinal, and the VS PATH/INCLUDE/LIB.
CALL "%HERE%\..\..\cmd\options.cmd" %*
CALL "%HERE%\..\..\cmd\setup.cmd" %*

SET DIR_CMDS=%DIR_REPO%\build\cmd
SET DIR_DIST=%DIR_REPO%\build\dist
SET DIR_SELF=%DIR_REPO%\target\dist_selftest
SET CL_LOG=%TEMP%\fl_dist_selftest_cl.tmp

:: clean: remove the scratch tree and exit.
IF %clean% EQU 1   GOTO :CLEAN
IF %cleanall% EQU 1 GOTO :CLEAN
IF %cleanplat% EQU 1 GOTO :CLEAN

IF EXIST "%DIR_SELF%" RD /S /Q "%DIR_SELF%"
MD "%DIR_SELF%"

SET FAILS=0

:: =======================================================================
::  exception_service  — single-binary platform exception service (FL_PLATFORM_BUILD)
::
::  The exception_service package ships fl_ + fla_ + flp_. A standalone binary
::  acts as the platform: build /DFL_PLATFORM_BUILD (so fl_try.h selects the
::  self-contained flp_ macros) and compile fl_ + flp_ ONLY. The fla_ stub must
::  be excluded -- it would both abort at runtime and clash with flp_ on the
::  shared exception entry points -- so this block lists sources explicitly
::  rather than globbing src\*.c.
:: =======================================================================
ECHO.
ECHO ============================================================
ECHO  exception_service
ECHO ============================================================
CALL "%DIR_CMDS%\exception_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist]    FAILED & SET /A FAILS+=1 & GOTO :after_exc )
SET INTO=%DIR_SELF%\exception_service
CALL :IMPORT "%DIR_REPO%\dist\exception_service" "%INTO%"
IF ERRORLEVEL 1 ( ECHO   [import]  FAILED & SET /A FAILS+=1 & GOTO :after_exc )
SET OBJ=%INTO%\_obj
IF NOT EXIST "%OBJ%" MD "%OBJ%"
cl %CommonCompilerFlagsFinal% /DFL_PLATFORM_BUILD /DFL_EMBEDDED /wd4456 ^
    /I"%INTO%\include" ^
    "%INTO%\src\fl_exception_service.c" ^
    "%INTO%\src\flp_exception_service.c" ^
    "%HERE%\exception_smoke_test.c" ^
    /Fo:"%OBJ%\\" /Fd:"%INTO%\exception_smoke.pdb" /Fe:"%INTO%\exception_smoke.exe" ^
    /link %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%CL_LOG%" 2>&1
IF ERRORLEVEL 1 ( ECHO   [compile] FAILED & TYPE "%CL_LOG%" & SET /A FAILS+=1 & GOTO :after_exc )
"%INTO%\exception_smoke.exe"
IF ERRORLEVEL 1 ( ECHO   [run]     FAILED & SET /A FAILS+=1 & GOTO :after_exc )
ECHO   [ok]
:after_exc

:: =======================================================================
::  log_service  — application-side service test (unity-includes fla_log_service.c)
:: =======================================================================
ECHO.
ECHO ============================================================
ECHO  log_service
ECHO ============================================================
CALL "%DIR_CMDS%\log_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist]    FAILED & SET /A FAILS+=1 & GOTO :after_log )
SET INTO=%DIR_SELF%\log_service
CALL :IMPORT "%DIR_REPO%\dist\log_service" "%INTO%"
IF ERRORLEVEL 1 ( ECHO   [import]  FAILED & SET /A FAILS+=1 & GOTO :after_log )
SET OBJ=%INTO%\_obj
IF NOT EXIST "%OBJ%" MD "%OBJ%"
:: The test unity-includes fla_log_service.c, so do NOT compile src\*.c too
:: (that would duplicate its symbols). Add <root>\src so the quoted include
:: resolves to the package copy.
cl %CommonCompilerFlagsFinal% /experimental:c11atomics /DFL_EMBEDDED ^
    /I"%INTO%\include" /I"%INTO%\src" ^
    "%HERE%\log_consumer_test.c" ^
    /Fo:"%OBJ%\\" /Fd:"%INTO%\log_consumer.pdb" /Fe:"%INTO%\log_consumer.exe" ^
    /link %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%CL_LOG%" 2>&1
IF ERRORLEVEL 1 ( ECHO   [compile] FAILED & TYPE "%CL_LOG%" & SET /A FAILS+=1 & GOTO :after_log )
"%INTO%\log_consumer.exe"
IF ERRORLEVEL 1 ( ECHO   [run]     FAILED & SET /A FAILS+=1 & GOTO :after_log )
ECHO   [ok]
:after_log

:: =======================================================================
::  memory  — single-binary platform memory-service assembly + fault injection
:: =======================================================================
ECHO.
ECHO ============================================================
ECHO  memory
ECHO ============================================================
CALL "%DIR_CMDS%\fault_memory_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist]    FAILED & SET /A FAILS+=1 & GOTO :after_mem )
SET INTO=%DIR_SELF%\memory
CALL :IMPORT "%DIR_REPO%\dist\fault_memory_service" "%INTO%"
IF ERRORLEVEL 1 ( ECHO   [import]  FAILED & SET /A FAILS+=1 & GOTO :after_mem )
SET OBJ=%INTO%\_obj
IF NOT EXIST "%OBJ%" MD "%OBJ%"
CALL :COLLECT_SRCS "%INTO%\src"
cl %CommonCompilerFlagsFinal% /experimental:c11atomics /wd4456 /DFL_EMBEDDED /DFL_PLATFORM_BUILD ^
    /I"%INTO%\include" /I"%INTO%\src" ^
    !SRCS! ^
    "%INTO%\src\fnv\*.c" ^
    "%HERE%\memory_consumer_test.c" ^
    /Fo:"%OBJ%\\" /Fd:"%INTO%\memory_consumer.pdb" /Fe:"%INTO%\memory_consumer.exe" ^
    /link %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%CL_LOG%" 2>&1
IF ERRORLEVEL 1 ( ECHO   [compile] FAILED & TYPE "%CL_LOG%" & SET /A FAILS+=1 & GOTO :after_mem )
"%INTO%\memory_consumer.exe"
IF ERRORLEVEL 1 ( ECHO   [run]     FAILED & SET /A FAILS+=1 & GOTO :after_mem )
ECHO   [ok]
:after_mem

:: =======================================================================
::  timer_service  — consumer-side injection over its exception_service dependency
:: =======================================================================
ECHO.
ECHO ============================================================
ECHO  timer_service
ECHO ============================================================
CALL "%DIR_CMDS%\exception_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist]    FAILED & SET /A FAILS+=1 & GOTO :after_timer )
CALL "%DIR_CMDS%\timer_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist]    FAILED & SET /A FAILS+=1 & GOTO :after_timer )
SET INTO=%DIR_SELF%\timer
:: timer_service declares SVC_DEPENDS=exception_service: import the dependency
:: first, then layer the package into the same tree (fl_import checks the dep).
CALL :IMPORT "%DIR_REPO%\dist\exception_service" "%INTO%"
IF ERRORLEVEL 1 ( ECHO   [import]  FAILED & SET /A FAILS+=1 & GOTO :after_timer )
CALL :IMPORT_ADD "%DIR_REPO%\dist\timer_service" "%INTO%"
IF ERRORLEVEL 1 ( ECHO   [import]  FAILED & SET /A FAILS+=1 & GOTO :after_timer )
SET OBJ=%INTO%\_obj
IF NOT EXIST "%OBJ%" MD "%OBJ%"
:: Consumer mode (no FL_PLATFORM_BUILD): fl_timer.h reads g_fla_timer_service.
:: fla_exception_service.c supplies fl_throw_assertion; flp_exception_service.c
:: is deliberately NOT compiled (one definition per binary).
cl %CommonCompilerFlagsFinal% /DFL_EMBEDDED /wd4456 ^
    /I"%INTO%\include" ^
    "%INTO%\src\fl_exception_service.c" ^
    "%INTO%\src\fla_exception_service.c" ^
    "%INTO%\src\flp_timer_service.c" ^
    "%INTO%\src\fla_timer_service.c" ^
    "%HERE%\timer_consumer_test.c" ^
    /Fo:"%OBJ%\\" /Fd:"%INTO%\timer_consumer.pdb" /Fe:"%INTO%\timer_consumer.exe" ^
    /link %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%CL_LOG%" 2>&1
IF ERRORLEVEL 1 ( ECHO   [compile] FAILED & TYPE "%CL_LOG%" & SET /A FAILS+=1 & GOTO :after_timer )
"%INTO%\timer_consumer.exe"
IF ERRORLEVEL 1 ( ECHO   [run]     FAILED & SET /A FAILS+=1 & GOTO :after_timer )
ECHO   [ok]
:after_timer

:: =======================================================================
::  file_service  — single-binary platform assembly over its memory_service dependency
:: =======================================================================
ECHO.
ECHO ============================================================
ECHO  file_service
ECHO ============================================================
CALL "%DIR_CMDS%\memory_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist]    FAILED & SET /A FAILS+=1 & GOTO :after_file )
CALL "%DIR_CMDS%\file_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist]    FAILED & SET /A FAILS+=1 & GOTO :after_file )
SET INTO=%DIR_SELF%\file
:: file_service declares SVC_DEPENDS=memory_service: import the dependency
:: first, then layer the package into the same tree (fl_import checks the dep).
CALL :IMPORT "%DIR_REPO%\dist\memory_service" "%INTO%"
IF ERRORLEVEL 1 ( ECHO   [import]  FAILED & SET /A FAILS+=1 & GOTO :after_file )
CALL :IMPORT_ADD "%DIR_REPO%\dist\file_service" "%INTO%"
IF ERRORLEVEL 1 ( ECHO   [import]  FAILED & SET /A FAILS+=1 & GOTO :after_file )
SET OBJ=%INTO%\_obj
IF NOT EXIST "%OBJ%" MD "%OBJ%"
CALL :COLLECT_SRCS "%INTO%\src"
cl %CommonCompilerFlagsFinal% /experimental:c11atomics /wd4456 /DFL_EMBEDDED /DFL_PLATFORM_BUILD ^
    /I"%INTO%\include" /I"%INTO%\src" ^
    !SRCS! ^
    "%HERE%\file_consumer_test.c" ^
    /Fo:"%OBJ%\\" /Fd:"%INTO%\file_consumer.pdb" /Fe:"%INTO%\file_consumer.exe" ^
    /link %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%CL_LOG%" 2>&1
IF ERRORLEVEL 1 ( ECHO   [compile] FAILED & TYPE "%CL_LOG%" & SET /A FAILS+=1 & GOTO :after_file )
"%INTO%\file_consumer.exe"
IF ERRORLEVEL 1 ( ECHO   [run]     FAILED & SET /A FAILS+=1 & GOTO :after_file )
ECHO   [ok]
:after_file

IF EXIST "%CL_LOG%" DEL "%CL_LOG%" 2> NUL

ECHO.
ECHO ============================================================
IF %FAILS% EQU 0 (
    ECHO  Distribution self-test: ALL PACKAGES PASSED
    ECHO ============================================================
    ENDLOCAL
    EXIT /B 0
)
ECHO  Distribution self-test: %FAILS% PACKAGE(S) FAILED
ECHO ============================================================
ENDLOCAL
EXIT /B 1

:: -----------------------------------------------------------------------
:: :IMPORT <from-package-dir> <into-tree>  — wipe and import via fl_import.ps1
:: -----------------------------------------------------------------------
:IMPORT
IF EXIST "%~2" RD /S /Q "%~2"
powershell -NoProfile -ExecutionPolicy Bypass -File "%DIR_DIST%\fl_import.ps1" -From "%~1" -Into "%~2"
EXIT /B %ERRORLEVEL%

:: -----------------------------------------------------------------------
:: :IMPORT_ADD <from-package-dir> <into-tree>  — layer a package into an
:: existing tree WITHOUT wiping, so a dependency imported by :IMPORT survives
:: (this is how a dependent package lands on top of its dependency).
:: -----------------------------------------------------------------------
:IMPORT_ADD
powershell -NoProfile -ExecutionPolicy Bypass -File "%DIR_DIST%\fl_import.ps1" -From "%~1" -Into "%~2"
EXIT /B %ERRORLEVEL%

:: -----------------------------------------------------------------------
:: :COLLECT_SRCS <src-dir>  — set SRCS to every *.c in <src-dir> EXCEPT files
:: that are unity-#included by another source. region_windows.c is included by
:: region_os.c, so compiling it as its own TU duplicates new_region/commit/etc.
:: The library's own build compiles region_os.c and never region_windows.c.
:: The lock backends (win32_lock.c, generic_lock.c) are unity-#included by
:: lock_os.c the same way.
:: -----------------------------------------------------------------------
:COLLECT_SRCS
SET SRCS=
FOR %%F IN ("%~1\*.c") DO (
    IF /I NOT "%%~nxF"=="region_windows.c" IF /I NOT "%%~nxF"=="win32_lock.c" IF /I NOT "%%~nxF"=="generic_lock.c" SET SRCS=!SRCS! "%%F"
)
EXIT /B 0

:CLEAN
IF EXIST "%DIR_SELF%" (
    ECHO Removing %DIR_SELF%
    RD /S /Q "%DIR_SELF%"
) ELSE (
    ECHO Nothing to clean.
)
ENDLOCAL
EXIT /B 0
