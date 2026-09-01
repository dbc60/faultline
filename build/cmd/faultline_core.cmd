@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Builds faultline_core.lib -- the OS-free application layer (test driver
:: + command layer + reporting) as a unity static library.
::
:: Compiled WITHOUT /DFL_PLATFORM_BUILD: the core reaches memory/log/exception/
:: timer/file/module capabilities only through services injected via the
:: FLPlatformAPI, never through platform symbols directly. The platform exe
:: (faultline_split.cmd) links this lib; dependency direction is platform->core.

SET PROJECT_NAME="Faultline Core"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*
CALL %DIR_CMD%\setup.cmd %*

:: cl and lib output capture, named per script inside this configuration's obj
:: directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%~n0_cl_out.tmp"

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\faultline_core.ctm
)

:: The unity TU is entirely first-party, so it takes whichever dialect the
:: caller picked with cxx.
SET "CORE_FLAGS=%CommonCompilerFlagsFinal%"
IF %cxx% EQU 1 SET "CORE_FLAGS=%CommonCompilerFlagsFinalCXX%"

IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build %PROJECT_NAME% ^(unity, OS-free^): faultline_core.lib
    )
    cl %CORE_FLAGS% /wd4200 /wd4115 /wd4456 /DFL_EMBEDDED /c ^
    /I%DIR_INCLUDE% /I%DIR_REPO%\src /I"%DIR_THIRD_PARTY%" ^
    /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    %DIR_REPO%\app\faultline\faultline_core_unity.c ^
    /Fo:%DIR_OUT_OBJ%\faultline_core.obj ^
    /Fd:%DIR_OUT_BIN%\faultline_core.pdb > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to compile %PROJECT_NAME% ^(unity^)
        GOTO :ERROR
    )
    del "%TEMP_OUT%"

    lib /NOLOGO /OUT:%DIR_OUT_LIB%\faultline_core.lib ^
    %DIR_OUT_OBJ%\faultline_core.obj > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to archive faultline_core.lib
        GOTO :ERROR
    )
    del "%TEMP_OUT%"
)

if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline_core.ctm %errorlevel%
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline_core.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
