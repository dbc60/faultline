@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Standard (non-unity) build of the FaultLine driver: each source compiled as a
:: separate translation unit and linked. Builds std_faultline_tests.dll and
:: std_faultline.exe, linking the prebuilt sqlite3.obj / cwalk.obj and sharing
:: faultline_test_data.dll produced by faultline_fixtures.cmd. Run
:: faultline_fixtures.cmd first (all.cmd does this automatically).
:: Timing (std_faultline.ctm) covers only the standard FaultLine compiles, so it
:: is directly comparable to faultline.ctm (the unity build of the same code).

SET PROJECT_NAME="Std Faultline"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
SET DIR_LOCAL=%DIR_CMD%\local
CALL %DIR_CMD%\options.cmd %*

CALL %DIR_CMD%\setup.cmd %*

:: The build defaults to debug unless release is explicitly passed in
IF NOT "%release%"=="" (
    if %release% EQU 1 (
        SET REL_OPT=release
    )
)

:: After 'clean' the shared fixtures are gone; rebuild them (untimed) so the
:: timed compile below can link sqlite3.obj / cwalk.obj and the test step can
:: load faultline_test_data.dll. all.cmd builds fixtures up front, so this is a
:: no-op there and on incremental rebuilds.
IF %build% EQU 1 IF NOT EXIST %DIR_OUT_OBJ%\sqlite3.obj (
    CALL %DIR_CMD%\faultline_fixtures.cmd build
    IF ERRORLEVEL 1 GOTO :ERROR
)

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\std_faultline.ctm
)

IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %PROJECT_NAME% test suite ^(non-unity^): std_faultline_tests.dll
    )
    cl %CommonCompilerFlagsFinal% /MP /wd4456 /wd4200 /wd4115 ^
    /I"%DIR_INCLUDE%" /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    /DDLL_BUILD ^
    %DIR_REPO%\src\faultline_tests.c ^
    %DIR_REPO%\src\faultline_test.c %DIR_REPO%\src\faultline_sqlite_test.c ^
    %DIR_REPO%\src\arena.c %DIR_REPO%\src\arena_dbg.c %DIR_REPO%\src\arena_malloc.c ^
    %DIR_REPO%\src\buffer.c %DIR_REPO%\src\digital_search_tree.c ^
    %DIR_REPO%\src\fault_injector.c %DIR_REPO%\src\fla_memory_service.c ^
    %DIR_REPO%\src\faultline_context.c %DIR_REPO%\src\faultline_driver.c ^
    %DIR_REPO%\src\faultline_sqlite.c %DIR_REPO%\src\fl_exception_service.c ^
    %DIR_REPO%\src\fl_threads.c %DIR_REPO%\src\fla_exception_service.c ^
    %DIR_REPO%\src\fla_log_service.c %DIR_REPO%\src\region.c ^
    %DIR_REPO%\src\region_node.c %DIR_REPO%\src\region_os.c ^
    %DIR_REPO%\src\set.c %DIR_REPO%\src\win_timer.c ^
    %DIR_THIRD_PARTY%\fnv\FNV64.c %DIR_REPO%\src\fla_timer_service.c ^
    %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\std_faultline_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% /LIBPATH:%DIR_OUT_LIB% ^
    /OUT:%DIR_OUT_BIN%\std_faultline_tests.dll /IMPLIB:%DIR_OUT_LIB%\std_faultline_tests.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% test suite ^(non-unity^)
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    IF %verbose% EQU 1 (
        ECHO.
        ECHO Build the Faultline Test Program ^(non-unity^): std_faultline.exe
    )
    cl %CommonCompilerFlagsFinal% /MP /wd4200 /wd4115 /wd4456 /DFL_PLATFORM_BUILD ^
    /I%DIR_INCLUDE% /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    /I%DIR_REPO%\src ^
    %DIR_REPO%\app\faultline\main_windows.c ^
    %DIR_REPO%\app\faultline\faultline_commands.c ^
    %DIR_REPO%\app\faultline\command_run.c ^
    %DIR_REPO%\app\faultline\command_show.c ^
    %DIR_REPO%\app\faultline\command_baseline.c ^
    %DIR_REPO%\app\faultline\command_help.c ^
    %DIR_REPO%\src\arena.c %DIR_REPO%\src\arena_dbg.c %DIR_REPO%\src\arena_malloc.c ^
    %DIR_REPO%\src\buffer.c %DIR_REPO%\src\command.c ^
    %DIR_REPO%\src\digital_search_tree.c %DIR_REPO%\src\fault_injector.c ^
    %DIR_REPO%\src\faultline_context.c %DIR_REPO%\src\faultline_driver.c ^
    %DIR_REPO%\src\faultline_sqlite.c %DIR_REPO%\src\fl_exception_service.c ^
    %DIR_REPO%\src\fl_threads.c %DIR_REPO%\src\flp_exception_service.c ^
    %DIR_REPO%\src\flp_log_service.c %DIR_REPO%\src\flp_memory_service.c ^
    %DIR_REPO%\src\flp_fault_memory_service.c %DIR_REPO%\src\flp_timer_service.c %DIR_REPO%\src\output_junit.c ^
    %DIR_REPO%\src\region.c %DIR_REPO%\src\region_node.c %DIR_REPO%\src\region_os.c ^
    %DIR_REPO%\src\set.c %DIR_REPO%\src\win_timer.c ^
    %DIR_THIRD_PARTY%\fnv\FNV64.c ^
    %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\std_faultline.pdb /Fe:%DIR_OUT_BIN%\std_faultline.exe /link ^
    %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% Test Program ^(non-unity^)
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"
)
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\std_faultline.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME% unit tests
        ECHO.
    )
    pushd %DIR_OUT_BIN%
    .\std_faultline.exe run std_faultline_tests.dll
    .\std_faultline.exe show results --limit 1
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\std_faultline.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
