@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: build_test_dll.cmd - Shared helper: compile one test DLL and optionally run
::   it with but_driver.exe. Called by individual component scripts; not
::   intended to be invoked directly.
::
:: Usage:
::   CALL build_test_dll.cmd <name> <ctm> <src> <dll> <extra> [build options...]
::
:: Parameters:
::   %1  Display name for progress messages (e.g. "Arena")
::   %2  Stem for the ctime metrics\vs file   (e.g. "arena" -> metrics\vs/arena.ctm)
::   %3  Source file under src\            (e.g. "arena_tests.c")
::   %4  Output DLL stem, without extension (e.g. "arena_tests")
::   %5  Extra compiler flags, or "" for none (e.g. "/wd4456")
::   %6+ Build options forwarded verbatim to options.cmd / setup.cmd
::       (build, debug, release, clean, cleanall, test, verbose, timed, ...)

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

:: Collect remaining args (build options) into _BTDLL_ARGS.
:: Note: %* is not updated by SHIFT in batch, so we collect manually.
SET "_BTDLL_ARGS="
:_btdll_collect
IF "%~1"=="" GOTO :_btdll_done
SET "_BTDLL_ARGS=%_BTDLL_ARGS% %~1"
SHIFT
GOTO :_btdll_collect
:_btdll_done

TITLE %_BTDLL_NAME%

CALL %_BTDLL_DIR%\options.cmd %_BTDLL_ARGS%
CALL %_BTDLL_DIR%\setup.cmd %_BTDLL_ARGS%

:: Track release option so we can pass it to but_driver.cmd if needed
SET "REL_OPT="
IF NOT "%release%"=="" (
    if %release% EQU 1 (
        SET REL_OPT=release
    )
)

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin "metrics\vs\%_BTDLL_CTM%.ctm"
)

IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %_BTDLL_NAME% test suite
    )
    cl %CommonCompilerFlagsFinal% %_BTDLL_EXTRA% /I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% ^
    /DDLL_BUILD %DIR_REPO%\src\%_BTDLL_SRC% ^
    /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_LIB%\%_BTDLL_DLL%.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\%_BTDLL_DLL%.dll ^
    /IMPLIB:%DIR_OUT_LIB%\%_BTDLL_DLL%.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %_BTDLL_NAME% test suite
        GOTO :_btdll_error
    )
    del "%TEMP%\cl_out.tmp"
)

if %test% EQU 1 (
    IF NOT EXIST "%DIR_OUT_BIN%\but_driver.exe" (
        call %_BTDLL_DIR%\but_driver.cmd build %REL_OPT%
        if errorlevel 1 (
            GOTO :_btdll_error
        )
    )
)

if %timed% EQU 1 (
    ctime.exe -end "metrics\vs\%_BTDLL_CTM%.ctm" %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run the %_BTDLL_NAME% unit tests
    )
    pushd %DIR_OUT_BIN%
    but_driver.exe %_BTDLL_DLL%.dll
    popd
)
GOTO :_btdll_success

:_btdll_error
if %timed% EQU 1 (
    ctime.exe -end "metrics\vs\%_BTDLL_CTM%.ctm" %errorlevel%
)
EXIT /B 1

:_btdll_success
ENDLOCAL
EXIT /B 0
