@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
SET PROJECT_NAME="Arena Benchmark"
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
    ctime.exe -begin metrics\vs\arena_bench.ctm
)

CALL %DIR_CMD%\setup.cmd %*

:: Build the project: one driver with synchronized-arena support compiled in,
:: and one with the project default (FL_ARENA_SYNCHRONIZED undefined).
IF %build% EQU 1 (
    IF %verbose% EQU 1 (
        ECHO Build %PROJECT_NAME%
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% ^
    /I%DIR_REPO%\src /DFL_PLATFORM_BUILD /DFL_ARENA_SYNCHRONIZED ^
    %DIR_REPO%\app\arena_bench\arena_bench_main.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\arena_bench.pdb /Fe:%DIR_OUT_BIN%\arena_bench.exe /link ^
    %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% Program
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    IF %verbose% EQU 1 (
        ECHO Build %PROJECT_NAME% without synchronized-arena support
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% ^
    /I%DIR_REPO%\src /DFL_PLATFORM_BUILD ^
    %DIR_REPO%\app\arena_bench\arena_bench_main.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\arena_bench_nosync.pdb ^
    /Fe:%DIR_OUT_BIN%\arena_bench_nosync.exe /link ^
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
    ctime.exe -end metrics\vs\arena_bench.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME%
    )
    echo Running %DIR_OUT_BIN%\arena_bench.exe
    %DIR_OUT_BIN%\arena_bench.exe
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\arena_bench.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
