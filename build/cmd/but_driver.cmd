@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
SET PROJECT_NAME="BUT Test Driver"
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
    ctime.exe -begin metrics\vs\but_test_driver.ctm
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
    IF %verbose% EQU 1 (
        ECHO Build the %PROJECT_NAME% test suite
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% ^
    /DDLL_BUILD %DIR_REPO%\src\but_tests.c ^
    /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_LIB%\but_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\but_tests.dll ^
    /IMPLIB:%DIR_OUT_LIB%\but_tests.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME%
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    if %verbose% EQU 1 (
        ECHO.
        ECHO Build the %PROJECT_NAME% driver test-data DLL
    )
    cl %CommonCompilerFlagsFinal% /I"%DIR_INCLUDE%" /I%DIR_THIRD_PARTY% ^
    /DDLL_BUILD /DFL_BUILD_DRIVER ^
    %DIR_REPO%\src\but_test_data.c ^
    /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_BIN%\but_test_data.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\but_test_data.dll ^
    /IMPLIB:%DIR_OUT_LIB%\but_test_data.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% driver's test-data DLL
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    IF %verbose% EQU 1 (
        ECHO.
        ECHO Build the %PROJECT_NAME% Driver
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% ^
    /DFL_BUILD_DRIVER /DFL_EMBEDDED ^
    %DIR_REPO%\cmd\but\win32_main.c  /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\but_driver.pdb /Fe:%DIR_OUT_BIN%\but_driver.exe /link ^
    %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% Program
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"
)
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\but_test_driver.ctm %errorlevel%
)

:: Run the tests
if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO.
        ECHO Run the %PROJECT_NAME%
        ECHO.
    )
    echo running %DIR_OUT_BIN%\but_driver.exe but_tests.dll
    pushd %DIR_OUT_BIN%
    but_driver.exe but_tests.dll
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\but_test_driver.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
