@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.

SET PROJECT_NAME="Index"
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
    ctime.exe -begin metrics\vs\index.ctm
)

CALL %DIR_CMD%\setup.cmd %*

:: The build defaults to debug unless release is explicitly passed in
IF NOT "%release%"=="" (
    if %release% EQU 1 (
        SET REL_OPT=release
    )
)

:: Build the generic version of the test suite
IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the Generic %PROJECT_NAME% test suite: %*
    )
    cl %CommonCompilerFlagsFinal% /I%DIR_INCLUDE% /DDLL_BUILD ^
    %DIR_REPO%\src\index_generic_tests.c /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_LIB%\index_generic_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\index_generic_tests.dll ^
    /IMPLIB:%DIR_OUT_LIB%\index_generic_tests.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the Generic %PROJECT_NAME% test suite
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"
)

:: Build the Windows version of the test suite
IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the Windows %PROJECT_NAME% test suite: %*
    )
    cl %CommonCompilerFlagsFinal% /I%DIR_INCLUDE% /DDLL_BUILD ^
    %DIR_REPO%\src\index_windows_tests.c /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_LIB%\index_windows_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\index_windows_tests.dll ^
    /IMPLIB:%DIR_OUT_LIB%\index_windows_tests.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the Windows %PROJECT_NAME% test suite
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
    ctime.exe -end metrics\vs\index.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run the %PROJECT_NAME% unit tests
    )
    pushd %DIR_OUT_BIN%
    but_driver.exe index_generic_tests.dll
    but_driver.exe index_windows_tests.dll
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\index.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
