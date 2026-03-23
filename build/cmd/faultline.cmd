@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.

SET PROJECT_NAME="Faultline"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
SET DIR_LOCAL=%DIR_CMD%\local
CALL %DIR_CMD%\options.cmd %*

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\faultline.ctm
)

CALL %DIR_CMD%\setup.cmd %*

:: The build defaults to debug unless release is explicitly passed in
IF NOT "%release%"=="" (
    if %release% EQU 1 (
        SET REL_OPT=release
    )
)

:: Build the project
IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %PROJECT_NAME% test suite: %*
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /wd4200 /wd4115 ^
    /I"%DIR_INCLUDE%" /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    /DDLL_BUILD ^
    %DIR_REPO%\src\faultline_tests.c %DIR_THIRD_PARTY%\sqlite\sqlite3.c  ^
    %DIR_THIRD_PARTY%\cwalk\src\cwalk.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\faultline_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% /LIBPATH:%DIR_OUT_LIB% ^
    /OUT:%DIR_OUT_BIN%\faultline_tests.dll /IMPLIB:%DIR_OUT_LIB%\faultline_tests.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% test suite
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    if %verbose% EQU 1 (
        ECHO.
        ECHO Build the %PROJECT_NAME% driver test-data DLL
    )
    cl %CommonCompilerFlagsFinal% /I"%DIR_INCLUDE%" /I"%DIR_THIRD_PARTY%" /DFL_BUILD_DRIVER /DDLL_BUILD ^
    %DIR_REPO%\src\faultline_test_data.c ^
    /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_BIN%\faultline_test_data.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\faultline_test_data.dll ^
    /IMPLIB:%DIR_OUT_LIB%\faultline_test_data.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% driver test-data DLL
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    IF %verbose% EQU 1 (
        ECHO.
        ECHO Build the Faultline Test Program
    )
    cl %CommonCompilerFlagsFinal% /wd4200 /wd4115 /wd4456 /DFL_BUILD_DRIVER ^
    /I%DIR_INCLUDE% /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    %DIR_REPO%\cmd\faultline\main_windows.c %DIR_THIRD_PARTY%\sqlite\sqlite3.c ^
    %DIR_THIRD_PARTY%\cwalk\src\cwalk.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\faultline.pdb /Fe:%DIR_OUT_BIN%\faultline.exe /link ^
    %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% Test Program
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"
)
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME% unit tests
        ECHO.
    )
    pushd %DIR_OUT_BIN%
    faultline.exe run faultline_tests.dll
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
