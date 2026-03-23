@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\all.ctm
)

CALL %DIR_CMD%\setup.cmd %*

set "args="
for %%A in (%*) do (
    if /I not "%%~A"=="test" if /I not "%%~A"=="clean" if /I not "%%~A"=="cleanall" (
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

call %DIR_CMD%\faultline.cmd !args!
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
    faultline.exe run !JUNIT_OPT! ^
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
        buffer_tests.dll ^
        fnv_tests.dll ^
        math_tests.dll ^
        set_tests.dll ^
        timer_tests.dll ^
        fault_injector_tests.dll ^
        fl_assert_tests.dll ^
        command_tests.dll ^
        faultline_tests.dll ^
        malloc_cleanup_config_tests.dll
    faultline.exe show results --limit 21
    popd
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
