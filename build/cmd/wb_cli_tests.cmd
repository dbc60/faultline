@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Build the command-engine test suite (wb_cli_tests.dll). The suite compiles wb_cli.c
:: into its own translation unit and nothing else: the engine is deliberately free of any
:: worldbuilder domain knowledge, so unlike the sqlite and DAL suites this one needs no
:: database and no sqlite3.obj, and therefore never calls worldbuilder_fixtures.cmd. The
:: command tree that does know about projects and modules lives in
:: worldbuilder_cli_commands.c and is exercised by driving the built executable.
::
:: Like the other suites it compiles the core side only: DLL_BUILD makes FL_DECL_SPEC
:: dllexport so the fla_set_* setters are exported, and no FL_PLATFORM_BUILD is defined,
:: so the unified headers select the fla_ accessors. The driver injects the platform
:: services at load time.

SET PROJECT_NAME="Worldbuilder CLI Tests"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*

CALL %DIR_CMD%\setup.cmd %*
:: No supported Visual Studio for a requested build; setup.cmd said why.
IF ERRORLEVEL 1 EXIT /B 1

SET FAULTLINE_INCLUDE=%DIR_FL%\include

:: cl output capture, named per script inside this configuration's obj directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%~n0_cl_out.tmp"

IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %PROJECT_NAME% suite: wb_cli_tests.dll
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /wd4200 /wd4115 /DDLL_BUILD ^
    /I"%DIR_INCLUDE%" /I"%DIR_THIRD_PARTY%" /I"%FAULTLINE_INCLUDE%" ^
    %DIR_REPO%\src\wb_cli_tests.c ^
    %FL_CORE_SOURCES% /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\wb_cli_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% /LIBPATH:%DIR_OUT_LIB% ^
    /OUT:%DIR_OUT_BIN%\wb_cli_tests.dll /IMPLIB:%DIR_OUT_LIB%\wb_cli_tests.lib > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %PROJECT_NAME% suite
        GOTO :ERROR
    )
    del "%TEMP_OUT%"
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME% unit tests
        ECHO.
    )
    pushd %DIR_OUT_BIN%
    faultline.exe run wb_cli_tests.dll
    faultline.exe show results --limit 1
    popd
)
GOTO :SUCCESS

:ERROR
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
