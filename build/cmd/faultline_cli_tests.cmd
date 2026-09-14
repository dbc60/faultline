@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Build the command-tree test suite (faultline_cli_tests.dll). The suite compiles the
:: command tree and the library under it as one translation unit, and links the prebuilt
:: sqlite3.obj / cwalk.obj that faultline_fixtures.cmd produces.

SET PROJECT_NAME="Faultline CLI Tests"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*

CALL %DIR_CMD%\setup.cmd %*
:: No supported Visual Studio for a requested build; setup.cmd said why.
IF ERRORLEVEL 1 EXIT /B 1

:: cl output capture, named per script inside this configuration's obj directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%~n0_cl_out.tmp"

:: A nested build must not repeat the clean that setup.cmd already performed, and must
:: write into this configuration's output tree. Forward the options but strip the
:: clean/test verbs (test maps to build), the same filtering all.cmd uses.
set "args="
for %%A in (%*) do (
    if /I not "%%~A"=="test" if /I not "%%~A"=="clean" if /I not "%%~A"=="cleanall" if /I not "%%~A"=="cleanplat" (
        set "args=!args! %%~A"
    )
    if /I "%%~A"=="test" (
        set "args=!args! build"
    )
)

:: After 'clean' the shared fixtures are gone; rebuild them so the link below finds
:: sqlite3.obj / cwalk.obj.
IF %build% EQU 1 IF NOT EXIST %DIR_OUT_OBJ%\sqlite3.obj (
    CALL %DIR_CMD%\faultline_fixtures.cmd !args!
    IF ERRORLEVEL 1 GOTO :ERROR
)

IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %PROJECT_NAME% suite: faultline_cli_tests.dll
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /wd4200 /wd4115 /DDLL_BUILD ^
    /I"%DIR_INCLUDE%" /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    /I%DIR_REPO%\src /I%DIR_REPO%\app\faultline ^
    %DIR_REPO%\src\faultline_cli_tests.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\faultline_cli_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% /LIBPATH:%DIR_OUT_LIB% ^
    %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj ^
    /OUT:%DIR_OUT_BIN%\faultline_cli_tests.dll /IMPLIB:%DIR_OUT_LIB%\faultline_cli_tests.lib > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %PROJECT_NAME% suite
        GOTO :ERROR
    )
    del "%TEMP_OUT%"
)

if %test% EQU 1 (
    IF NOT EXIST "%DIR_OUT_BIN%\faultline.exe" (
        call %DIR_CMD%\faultline.cmd !args!
        if errorlevel 1 (
            GOTO :ERROR
        )
    )
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME% unit tests
        ECHO.
    )
    pushd %DIR_OUT_BIN%
    .\faultline.exe run faultline_cli_tests.dll
    .\faultline.exe result list --limit 1
    popd
)
GOTO :SUCCESS

:ERROR
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
