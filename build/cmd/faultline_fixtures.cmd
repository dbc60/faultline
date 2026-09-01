@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Shared, UNTIMED build fixtures common to both the unity (faultline.cmd) and
:: standard (std_faultline.cmd) driver builds:
::   - sqlite3.obj / cwalk.obj : large third-party TUs compiled identically by
::     every build model; prebuilt once here and linked by both so they don't
::     skew the unity-vs-standard timing comparison.
::   - faultline_test_data.dll : loaded by faultline_tests.dll via a hard-coded
::     name, so it is a shared runtime fixture, not a comparison target.

SET PROJECT_NAME="Faultline Fixtures"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
SET DIR_LOCAL=%DIR_CMD%\local
CALL %DIR_CMD%\options.cmd %*

CALL %DIR_CMD%\setup.cmd %*

:: cl output capture, named per script inside this configuration's obj directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%~n0_cl_out.tmp"

:: The build defaults to debug unless release is explicitly passed in
IF NOT "%release%"=="" (
    if %release% EQU 1 (
        SET REL_OPT=release
    )
)

IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build shared fixture object: sqlite3.obj
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /wd4200 /wd4115 ^
    /I"%DIR_THIRD_PARTY%" /c %DIR_THIRD_PARTY%\sqlite\sqlite3.c ^
    /Fo:%DIR_OUT_OBJ%\sqlite3.obj /Fd:%DIR_OUT_OBJ%\sqlite3.pdb > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build sqlite3.obj
        GOTO :ERROR
    )
    del "%TEMP_OUT%"

    if %verbose% EQU 1 (
        ECHO Build shared fixture object: cwalk.obj
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /wd4200 /wd4115 ^
    /I"%DIR_THIRD_PARTY%\cwalk\include" /c %DIR_THIRD_PARTY%\cwalk\src\cwalk.c ^
    /Fo:%DIR_OUT_OBJ%\cwalk.obj /Fd:%DIR_OUT_OBJ%\cwalk.pdb > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build cwalk.obj
        GOTO :ERROR
    )
    del "%TEMP_OUT%"

    if %verbose% EQU 1 (
        ECHO.
        ECHO Build the %PROJECT_NAME% driver test-data DLL
    )
    REM First-party, unlike the two third-party fixtures above, so it takes
    REM whichever dialect the caller picked with cxx.
    SET "TESTDATA_FLAGS=%CommonCompilerFlagsFinal%"
    IF %cxx% EQU 1 SET "TESTDATA_FLAGS=%CommonCompilerFlagsFinalCXX%"
    cl !TESTDATA_FLAGS! /I"%DIR_INCLUDE%" /I"%DIR_THIRD_PARTY%" /DFL_PLATFORM_BUILD /DDLL_BUILD ^
    %DIR_REPO%\src\faultline_test_data.c ^
    /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_BIN%\faultline_test_data.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\faultline_test_data.dll ^
    /IMPLIB:%DIR_OUT_LIB%\faultline_test_data.lib > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %PROJECT_NAME% driver test-data DLL
        GOTO :ERROR
    )
    del "%TEMP_OUT%"
)
GOTO :SUCCESS

:ERROR
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
