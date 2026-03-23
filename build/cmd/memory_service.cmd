@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
SET PROJECT_NAME="Memory Service"
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
    ctime.exe -begin metrics\vs\fl_memory_service.ctm
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
        ECHO.
        ECHO Build the %PROJECT_NAME% test suite
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /I%DIR_INCLUDE% /DDLL_BUILD ^
    %DIR_REPO%\src\flp_memory_service_tests.c ^
    /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_LIB%\flp_memory_service_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\flp_memory_service_tests.dll ^
    /IMPLIB:%DIR_OUT_LIB%\flp_memory_service_tests.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% test suite
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"
)

if %test% EQU 1 (
    IF NOT EXIST "%DIR_OUT_BIN%\but_driver.exe" (
        call %DIR_CMD%\but_driver.cmd build %REL_OPT%
        if errorlevel 1 (
            GOTO :ERROR
        )
    )
)
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\fl_memory_service.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run the %PROJECT_NAME% unit tests
        ECHO.
    )
    pushd %DIR_OUT_BIN%
    but_driver.exe flp_memory_service_tests.dll
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\fl_memory_service.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
