@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*

CALL %DIR_CMD%\setup.cmd %*
:: No supported Visual Studio for a requested build.
IF ERRORLEVEL 1 EXIT /B 1

:: Exit early if build is set to zero
IF %build% EQU 0 GOTO :SUCCESS

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\all.ctm
)

set "args="
for %%A in (%*) do (
    if /I not "%%~A"=="test" if /I not "%%~A"=="clean" if /I not "%%~A"=="cleanall" if /I not "%%~A"=="cleanplat" (
        set "args=!args! %%~A"
    )
    if /I "%%~A"=="test" (
        set "args=!args! build"
    )
)

call %DIR_CMD%\exception_service.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\but_driver.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\dlist.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\log_service.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\bits.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\region.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\chunk.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\digital_search_tree.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\index.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\arena.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\arena_pool.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\buffer.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\fnv.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\math.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\set.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\timer.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\fault_injector.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\fl_assert.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\command.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\faultline_fixtures.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\faultline.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\std_faultline.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\faultline_split.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\malloc_cleanup_config.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\faultline_lib.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\memory_service.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\file_service.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

call %DIR_CMD%\stream_service.cmd !args!
if errorlevel 1 (
    GOTO :ERROR
)

:: Code analysis is opt-in and runs after the build, so producing the binaries is
:: never delayed by it. The pass links nothing, so it adds no artifact to copy into
:: test\ below.
if %analyze% EQU 1 (
    call %DIR_CMD%\faultline_analyze.cmd !args!
    if errorlevel 1 (
        GOTO :ERROR
    )
)

IF NOT EXIST test MD test
copy /y %DIR_OUT_BIN%\*.exe test\ > NUL
copy /y %DIR_OUT_BIN%\*.dll test\ > NUL

if %timed% EQU 1 (
    ctime.exe -end metrics\vs\all.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO "Run all unit tests"
    )
    TITLE Unit Tests
    set "JUNIT_OPT="
    if %junit% EQU 1 set "JUNIT_OPT=--junit-xml junit.xml"
    pushd test
    .\faultline.exe run !JUNIT_OPT! ^
        fl_exception_tests.dll ^
        but_tests.dll ^
        dlist_tests.dll ^
        fl_log_service_tests.dll ^
        bits_tests.dll ^
        region_tests.dll ^
        chunk_tests.dll ^
        digital_search_tree_tests.dll ^
        index_generic_tests.dll ^
        index_windows_tests.dll ^
        arena_tests.dll ^
        arena_pool_tests.dll ^
        buffer_tests.dll ^
        fnv_tests.dll ^
        math_tests.dll ^
        set_tests.dll ^
        timer_tests.dll ^
        fault_injector_tests.dll ^
        fl_assert_tests.dll ^
        command_tests.dll ^
        faultline_tests.dll ^
        malloc_cleanup_config_tests.dll ^
        flp_memory_service_tests.dll ^
        flp_file_service_tests.dll ^
        flp_stream_service_tests.dll
    REM The driver returns 0 for ordinary test failures, so a non-zero status
    REM means it died or could not start. A crash takes down every suite listed
    REM after the one that crashed, and those suites report nothing at all -- the
    REM results table then backfills with older runs, which looks like a normal
    REM listing. Fail the build rather than let a truncated run pass for a
    REM complete one.
    set "_run_rc=!errorlevel!"
    .\faultline.exe show results --limit 25
    REM Split-architecture smoke: the same suites driven through the split host.
    REM Its log goes to faultline.log; the results table shows the runs.
    .\win32_faultline.exe run ^
        faultline_tests.dll ^
        flp_file_service_tests.dll ^
        flp_stream_service_tests.dll ^
        timer_tests.dll
    set "_split_rc=!errorlevel!"
    .\win32_faultline.exe show results --limit 4
    popd
    if not "!_run_rc!"=="0" (
        ECHO ALL.CMD ERROR: test driver exited with !_run_rc! -- suites listed 1>&2
        ECHO after the failure point did not run. 1>&2
        GOTO :ERROR
    )
    if not "!_split_rc!"=="0" (
        ECHO ALL.CMD ERROR: split host exited with !_split_rc! -- suites listed 1>&2
        ECHO after the failure point did not run. 1>&2
        GOTO :ERROR
    )
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\all.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
