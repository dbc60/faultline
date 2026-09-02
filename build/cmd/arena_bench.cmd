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

:: cl output capture, named per script inside this configuration's obj directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%~n0_cl_out.tmp"

:: Build sources as C code by default. If cxx is 1, then build sources as C++.
SET "BENCH_FLAGS=%CommonCompilerFlagsFinal%"
IF %cxx% EQU 1 SET "BENCH_FLAGS=%CommonCompilerFlagsFinalCXX%"

:: Build the project: one driver with synchronized-arena support compiled in,
:: and one with the project default (FL_ARENA_SYNCHRONIZED undefined).
IF %build% EQU 1 (
    IF %verbose% EQU 1 (
        ECHO Build %PROJECT_NAME%
    )
    cl %BENCH_FLAGS% /wd4456 /I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% ^
    /I%DIR_REPO%\src /DFL_PLATFORM_BUILD /DFL_ARENA_SYNCHRONIZED ^
    %DIR_REPO%\app\arena_bench\arena_bench_main.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\arena_bench.pdb /Fe:%DIR_OUT_BIN%\arena_bench.exe /link ^
    %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %PROJECT_NAME% Program
        GOTO :ERROR
    )
    del "%TEMP_OUT%"

    IF %verbose% EQU 1 (
        ECHO Build %PROJECT_NAME% without synchronized-arena support
    )
    cl %BENCH_FLAGS% /wd4456 /I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% ^
    /I%DIR_REPO%\src /DFL_PLATFORM_BUILD ^
    %DIR_REPO%\app\arena_bench\arena_bench_main.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\arena_bench_nosync.pdb ^
    /Fe:%DIR_OUT_BIN%\arena_bench_nosync.exe /link ^
    %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %PROJECT_NAME% Program
        GOTO :ERROR
    )
    del "%TEMP_OUT%"

    REM Keep a copy outside the target tree, which is what clean removes. A
    REM benchmark is only worth reading against its own history, so whatever a
    REM run leaves beside the executable -- captured output, a results file
    REM later on -- has to outlive a rebuild. REM, not ::, because a label
    REM inside a parenthesised block breaks cmd's parser.
    IF NOT EXIST "%DIR_REPO%\bench" MD "%DIR_REPO%\bench"
    COPY /Y "%DIR_OUT_BIN%\arena_bench.exe" "%DIR_REPO%\bench\arena_bench.exe" >NUL
    COPY /Y "%DIR_OUT_BIN%\arena_bench_nosync.exe" "%DIR_REPO%\bench\arena_bench_nosync.exe" >NUL
)
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\arena_bench.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME%
    )
    echo Running %DIR_REPO%\bench\arena_bench.exe
    PUSHD "%DIR_REPO%\bench"
    arena_bench.exe
    ECHO.
    arena_bench_nosync.exe
    POPD
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
