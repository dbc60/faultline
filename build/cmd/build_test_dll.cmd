@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: build_test_dll.cmd - Shared helper: compile one test DLL and optionally run
::   it with project.cmd's FL_TEST_DRIVER. Called by individual component
::   scripts; not intended to be invoked directly.
::
:: Usage:
::   CALL build_test_dll.cmd <name> <ctm> <src> <dll> <extra> [flags...] [build options...]
::
:: Parameters:
::   %1  Display name for progress messages (e.g. "Arena")
::   %2  Stem for the ctime file in DIR_METRICS (e.g. "arena" -> arena.ctm)
::   %3  Source file under DIR_SRC          (e.g. "arena_tests.c")
::   %4  Output DLL stem, without extension (e.g. "arena_tests")
::   %5  Extra compiler flags, or "" for none (e.g. "/wd4456")
::   %6+ Build options forwarded verbatim to options.cmd / setup.cmd
::       (build, debug, release, clean, cleanall, test, verbose, timed, ...)
::
:: Flags, accepted anywhere after %5:
::   --extra-src SRC  Extra source under DIR_SRC compiled and linked with the main
::                    source; may be repeated
::   --fl-core        Link FaultLine's FL_CORE_SOURCES (config.cmd), for a suite
::                    that does not #include them itself
::
:: Compiler includes and flags come from project.cmd's PROJECT_INCLUDES and
:: PROJECT_COMPILER_FLAGS. With test, the suite runs from the output bin directory
:: through FL_TEST_DRIVER, built first by FL_TEST_DRIVER_SCRIPT when one is named
:: and the driver is missing.

SET "_BTDLL_DIR=%~dp0"
SET "_BTDLL_DIR=%_BTDLL_DIR:~0,-1%"
SET "_BTDLL_NAME=%~1"
SET "_BTDLL_CTM=%~2"
SET "_BTDLL_SRC=%~3"
SET "_BTDLL_DLL=%~4"
SET "_BTDLL_EXTRA=%~5"
SHIFT
SHIFT
SHIFT
SHIFT
SHIFT

:: Collect the flags into _BTDLL_EXTRA_SRCS and _BTDLL_FL_CORE, and the remaining
:: args (build options) into _BTDLL_ARGS.
:: Note: %* is not updated by SHIFT in batch, so we collect manually.
SET "_BTDLL_ARGS="
SET "_BTDLL_EXTRA_SRCS="
SET _BTDLL_FL_CORE=0
:_btdll_collect
IF "%~1"=="" GOTO :_btdll_done
IF /I "%~1"=="--fl-core" GOTO :_btdll_fl_core
IF /I "%~1"=="--extra-src" GOTO :_btdll_extra_src
SET "_BTDLL_ARGS=%_BTDLL_ARGS% %~1"
SHIFT
GOTO :_btdll_collect

:_btdll_fl_core
SET _BTDLL_FL_CORE=1
SHIFT
GOTO :_btdll_collect

:_btdll_extra_src
IF "%~2"=="" (
    ECHO BUILD_TEST_DLL.CMD ERROR: --extra-src needs a source file 1>&2
    EXIT /B 1
)
SET "_BTDLL_EXTRA_SRCS=%_BTDLL_EXTRA_SRCS% %~2"
SHIFT
SHIFT
GOTO :_btdll_collect
:_btdll_done

TITLE %_BTDLL_NAME%

CALL %_BTDLL_DIR%\options.cmd %_BTDLL_ARGS%
CALL %_BTDLL_DIR%\setup.cmd %_BTDLL_ARGS%

:: cl output capture, named per test DLL inside this configuration's obj directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%_BTDLL_DLL%_cl_out.tmp"

:: The nested driver bootstrap must not repeat the clean that setup.cmd already
:: performed, and it must write into the same output tree this script runs the
:: driver from: setup.cmd derives target\<vs>\<platform>\<buildtype> from the
:: option flags, and a nested call starts them all at zero, so without the
:: options forwarded it would default to debug/x64/newest-VS and put the driver
:: in the wrong target directory. Forward the options but strip the clean/test
:: verbs (test maps to build), the same filtering all.cmd uses.
set "args="
for %%A in (%_BTDLL_ARGS%) do (
    if /I not "%%~A"=="test" if /I not "%%~A"=="clean" if /I not "%%~A"=="cleanall" if /I not "%%~A"=="cleanplat" (
        set "args=!args! %%~A"
    )
    if /I "%%~A"=="test" (
        set "args=!args! build"
    )
)

if %timed% EQU 1 (
    if NOT EXIST "%DIR_METRICS%" (
        md "%DIR_METRICS%"
    )
    ctime.exe -begin "%DIR_METRICS%\%_BTDLL_CTM%.ctm"
)

:: Build sources as C code by default. If cxx is 1, then build sources as C++.
SET "_BTDLL_FLAGS=%CommonCompilerFlagsFinal%"
IF %cxx% EQU 1 SET "_BTDLL_FLAGS=%CommonCompilerFlagsFinalCXX%"

:: Sources compiled and linked with the main source: --extra-src files, then
:: FL_CORE_SOURCES for --fl-core. All take the main source's dialect.
SET "_BTDLL_LINK_SRCS="
FOR %%S IN (%_BTDLL_EXTRA_SRCS%) DO SET "_BTDLL_LINK_SRCS=!_BTDLL_LINK_SRCS! %DIR_SRC%\%%S"
IF %_BTDLL_FL_CORE% EQU 1 SET "_BTDLL_LINK_SRCS=!_BTDLL_LINK_SRCS! %FL_CORE_SOURCES%"

IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %_BTDLL_NAME% test suite
    )
    cl %_BTDLL_FLAGS% %PROJECT_COMPILER_FLAGS% %_BTDLL_EXTRA% %PROJECT_INCLUDES% ^
    /DDLL_BUILD %DIR_SRC%\%_BTDLL_SRC% %_BTDLL_LINK_SRCS% ^
    /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_LIB%\%_BTDLL_DLL%.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\%_BTDLL_DLL%.dll ^
    /IMPLIB:%DIR_OUT_LIB%\%_BTDLL_DLL%.lib > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %_BTDLL_NAME% test suite
        GOTO :_btdll_error
    )
    del "%TEMP_OUT%"
)

if %test% EQU 1 (
    IF NOT EXIST "%FL_TEST_DRIVER%" (
        IF "%FL_TEST_DRIVER_SCRIPT%"=="" (
            ECHO BUILD_TEST_DLL.CMD ERROR: test driver "%FL_TEST_DRIVER%" not found 1>&2
            GOTO :_btdll_error
        )
        call %_BTDLL_DIR%\%FL_TEST_DRIVER_SCRIPT% !args!
        if errorlevel 1 (
            GOTO :_btdll_error
        )
    )
)

if %timed% EQU 1 (
    ctime.exe -end "%DIR_METRICS%\%_BTDLL_CTM%.ctm" %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run the %_BTDLL_NAME% unit tests
    )
    pushd %DIR_OUT_BIN%
    "%FL_TEST_DRIVER%" run %_BTDLL_DLL%.dll
    "%FL_TEST_DRIVER%" result list --limit 1
    "%FL_TEST_DRIVER%" failure list
    popd
)
GOTO :_btdll_success

:_btdll_error
if %timed% EQU 1 (
    ctime.exe -end "%DIR_METRICS%\%_BTDLL_CTM%.ctm" %errorlevel%
)
EXIT /B 1

:_btdll_success
ENDLOCAL
EXIT /B 0
