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
::
:: PREREQUISITES (the split does not compile until these land):
::   - headers:  include/platform_api.h, include/faultline/fl_timer_service.h,
::               include/faultline/fl_file_service.h
::   - seam #1:  faultline_driver.c times via FLTimerService (fctx->timer),
::               not start_win/stop_win/elapsed_win_seconds
::   - seam #2:  the driver allocates via the injected memory service, not direct
::               arena_malloc_throw on a platform arena
::   - seam #3:  core uses the fla_ exception macros (injected FLExceptionService),
::               not the flp_ variants
::   - seam #4:  output_junit.c writes via FLFileService, not fopen/FILE*
::   - new file: app/faultline/faultline_app_main.c (extracted from main_windows.c)

SET PROJECT_NAME="Faultline Core"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*
CALL %DIR_CMD%\setup.cmd %*

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\faultline_core.ctm
)

IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build %PROJECT_NAME% ^(unity, OS-free^): faultline_core.lib
    )
    cl %CommonCompilerFlagsFinal% /wd4200 /wd4115 /wd4456 /DFL_EMBEDDED /c ^
    /I%DIR_INCLUDE% /I%DIR_REPO%\src /I"%DIR_THIRD_PARTY%" ^
    /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    %DIR_REPO%\app\faultline\faultline_core_unity.c ^
    /Fo:%DIR_OUT_OBJ%\faultline_core.obj ^
    /Fd:%DIR_OUT_BIN%\faultline_core.pdb > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to compile %PROJECT_NAME% ^(unity^)
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    lib /NOLOGO /OUT:%DIR_OUT_LIB%\faultline_core.lib ^
    %DIR_OUT_OBJ%\faultline_core.obj > "%TEMP%\lib_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\lib_out.tmp"
        del "%TEMP%\lib_out.tmp"
        echo failed to archive faultline_core.lib
        GOTO :ERROR
    )
    del "%TEMP%\lib_out.tmp"
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
