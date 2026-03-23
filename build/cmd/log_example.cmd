@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.

SET PROJECT_NAME="Log Example"
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
    ctime.exe -begin metrics\vs\log_example.ctm
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
        ECHO Build %PROJECT_NAME%
    )
    cl %CommonCompilerFlagsFinal% /I%DIR_INCLUDE% ^
    %DIR_REPO%\cmd\log_example\main.c  /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\log_example.pdb /Fe:%DIR_OUT_BIN%\log_example.exe /link ^
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
    ctime.exe -end metrics\vs\log_example.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME%
    )
    echo Running %DIR_OUT_BIN%\log_example.exe
    %DIR_OUT_BIN%\log_example.exe
    type app.log
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\log_example.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
