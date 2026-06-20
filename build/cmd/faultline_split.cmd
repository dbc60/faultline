@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Split-architecture build of faultline.exe: compiles the Win32 platform layer
:: (win32_faultline_unity.c) and links it against faultline_core.lib plus the
:: prebuilt sqlite3.obj / cwalk.obj. This is the platform/app counterpart to the
:: monolithic faultline.cmd, which is left intact for the current architecture.
::
:: Pipeline:
::   1. faultline_fixtures.cmd  -> sqlite3.obj / cwalk.obj / faultline_test_data.dll
::   2. faultline_core.cmd      -> faultline_core.lib (the OS-free app layer)
::   3. this script             -> win32_faultline.exe (the platform layer)
::
:: See faultline_core.cmd for the prerequisite seam refactors; until those land
:: this build will not link (the core will reference platform symbols directly).

SET PROJECT_NAME="Faultline (split)"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
SET DIR_LOCAL=%DIR_CMD%\local
CALL %DIR_CMD%\options.cmd %*
CALL %DIR_CMD%\setup.cmd %*

IF NOT "%release%"=="" (
    if %release% EQU 1 (
        SET REL_OPT=release
    )
)

:: Shared fixtures (sqlite3.obj / cwalk.obj) must exist before the link step.
IF %build% EQU 1 IF NOT EXIST %DIR_OUT_OBJ%\sqlite3.obj (
    CALL %DIR_CMD%\faultline_fixtures.cmd build
    IF ERRORLEVEL 1 GOTO :ERROR
)

:: Build the OS-free application layer first.
IF %build% EQU 1 (
    CALL %DIR_CMD%\faultline_core.cmd %*
    IF ERRORLEVEL 1 GOTO :ERROR
)

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\faultline_split.ctm
)

:: Build the platform layer and link the core lib + prebuilt objs.
IF %build% EQU 1 (
    IF %verbose% EQU 1 (
        ECHO.
        ECHO Build the %PROJECT_NAME% platform layer ^(unity^): faultline.exe
    )
    cl %CommonCompilerFlagsFinal% /wd4200 /wd4115 /wd4456 /DFL_PLATFORM_BUILD ^
    /I%DIR_INCLUDE% /I%DIR_REPO%\src /I"%DIR_THIRD_PARTY%" ^
    /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    %DIR_REPO%\app\faultline\win32_faultline_unity.c ^
    %DIR_OUT_LIB%\faultline_core.lib ^
    %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\faultline.pdb /Fe:%DIR_OUT_BIN%\faultline.exe /link ^
    %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% platform layer ^(unity^)
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"
)

if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline_split.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME% unit tests
        ECHO.
    )
    pushd %DIR_OUT_BIN%
    .\faultline.exe run faultline_tests.dll
    .\faultline.exe show results --limit 1
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline_split.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
