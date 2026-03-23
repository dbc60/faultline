@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.

SET PROJECT_NAME="Win32 Intrinsics"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\win32.ctm
)

CALL %DIR_CMD%\setup.cmd %*

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
        ECHO Build %PROJECT_NAME% test suite
    )
    cl %CommonCompilerFlagsFinal% /I%DIR_INCLUDE% /DDLL_BUILD ^
    "%DIR_REPO%\src\win32_intrinsics_butts.c" ^
    /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_BIN%\win32_intrinsics_butts.pdb ^
    /LD /link %CommonLinkerFlagsFinal% ^
    /OUT:%DIR_OUT_BIN%\win32_intrinsics_butts.dll ^
    /IMPLIB:%DIR_OUT_LIB%\win32_intrinsics_butts.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% test suite
        GOTO :EOF
    )
    del "%TEMP%\cl_out.tmp"
)

if %test% EQU 1 (
    IF NOT EXIST "%DIR_OUT_BIN%\faultline.exe" (
        call %DIR_CMD%\faultline.cmd build %REL_OPT%
        if errorlevel 1 (
            GOTO :ERROR
        )
    )
)
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\win32.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run the %PROJECT_NAME% unit tests
    )
    pushd %DIR_OUT_BIN%
    faultline.exe run win32_intrinsics_butts.dll
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\win32.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
