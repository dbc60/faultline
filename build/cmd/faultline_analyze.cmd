@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Run MSVC code analysis over the first-party translation units. Selected by the
:: "analyze" build option, which all.cmd forwards here after the build.
::
:: Analysis runs as a pass of its own rather than as a flag added to
:: CommonCompilerFlagsFinal because several ordinary cl lines name third-party sources
:: alongside first-party ones -- faultline_lib.cmd and both std_faultline.cmd lines
:: carry FNV64.c, faultline_lib.cmd carries sqlite3.c as well, and
:: faultline_fixtures.cmd compiles sqlite3.c and cwalk.c on their own. /external:I
:: applies to headers, not to primary sources, so it cannot exclude them.
::
:: Nothing here links, so this script needs no prebuilt objects and produces no
:: artifact beyond the report. Prefer debug (the default) over release: release adds
:: /GL, and analysis is per translation unit, so link-time code generation costs time
:: and reports nothing extra.
::
:: faultline_lib.cmd and std_faultline.cmd are not represented below. Both compile the
:: same library and driver sources the four unity translation units already pull in,
:: so the code is covered; only the mixed command lines are skipped.

SET PROJECT_NAME="FaultLine Analyze"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*

CALL %DIR_CMD%\setup.cmd %*
:: No supported Visual Studio for a requested build; setup.cmd said why.
IF ERRORLEVEL 1 EXIT /B 1

:: Nothing to analyze when the caller asked only for a clean. config.cmd runs only
:: when build is set, so ANALYZE_FLAGS would be empty here as well.
IF %build% EQU 0 GOTO :SUCCESS

:: cl output capture, named per script inside this configuration's obj directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%~n0_cl_out.tmp"

:: First-party include paths. The third-party directories are named only by
:: /external:I below: an /external:I directory takes part in the include search as
:: well, so a second /I for it would add nothing. faultline_tests_unity.c #includes
:: "fnv/FNV64.c", which resolves that way and is then kept out of the report by
:: /analyze:external-.
SET ANALYZE_INCLUDES=/I%DIR_INCLUDE% /I%DIR_REPO%\src

:: Header directories whose findings are not ours to fix. /external:env:INCLUDE covers
:: the Windows SDK and the CRT. Each third-party directory is named separately because
:: /external:I marks subdirectories as external but does not search them.
:: /external:anglebrackets is deliberately absent: first-party headers are included
:: with angle brackets too, so it would exclude the code being analyzed.
SET ANALYZE_EXTERNAL=/external:W0 /external:env:INCLUDE ^
    /external:I "%DIR_THIRD_PARTY%" /external:I "%DIR_THIRD_PARTY%\cwalk\include" ^
    /external:I "%DIR_THIRD_PARTY%\fnv"

:: The union of the /wd sets the build lines carry, applied to every unit rather than
:: per unit. All three are ordinary compiler warnings that some part of the build
:: already suppresses, and none is an analysis warning, so suppressing them everywhere
:: costs no finding. The build still enforces them where it always did.
SET ANALYZE_SUPPRESS=/wd4456 /wd4200 /wd4115

:: The unity translation units: one per binary the repository ships. Between them they
:: pull in the whole library, the driver, and the split host.
CALL :ANALYZE src\faultline_tests_unity.c "/DDLL_BUILD"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE app\faultline\main_unity_windows.c "/DFL_PLATFORM_BUILD"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE app\faultline\faultline_core_unity.c "/DFL_EMBEDDED"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE app\faultline\win32_faultline_unity.c "/DFL_PLATFORM_BUILD /DFL_EMBEDDED"
IF ERRORLEVEL 1 GOTO :ERROR

:: Translation units whose build scripts do not go through build_test_dll.cmd.
CALL :ANALYZE src\but_tests.c "/DDLL_BUILD"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE src\but_test_data.c "/DDLL_BUILD /DFL_PLATFORM_BUILD"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE app\but\win32_main.c "/DFL_PLATFORM_BUILD /DFL_EMBEDDED"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE src\index_generic_tests.c "/DDLL_BUILD"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE src\index_windows_tests.c "/DDLL_BUILD"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE src\faultline_test_data.c "/DFL_PLATFORM_BUILD /DDLL_BUILD"
IF ERRORLEVEL 1 GOTO :ERROR

CALL :ANALYZE src\malloc_cleanup_config_tests.c "/DDLL_BUILD"
IF ERRORLEVEL 1 GOTO :ERROR

:: The per-component suites built by build_test_dll.cmd. They are listed as bare stems
:: rather than as their own CALL lines because they differ in nothing this pass cares
:: about: each is one src\<stem>.c compiled /DDLL_BUILD. Add a stem here when a new
:: suite script is added beside the others.
SET SUITE_TUS=arena_tests arena_pool_tests bits_tests buffer_tests chunk_tests ^
    command_tests digital_search_tree_tests dlist_tests fault_injector_tests ^
    fl_assert_tests fl_exception_tests fl_log_service_tests flp_file_service_tests ^
    flp_memory_service_tests flp_stream_service_tests fnv_tests math_tests ^
    region_tests set_tests timer_tests

FOR %%T IN (%SUITE_TUS%) DO (
    CALL :ANALYZE src\%%T.c "/DDLL_BUILD"
    IF ERRORLEVEL 1 GOTO :ERROR
)

GOTO :SUCCESS

:: Analyze one translation unit. %1 is its path below the repository root, %2 its /D set.
::
:: Unlike every other script here this one always prints the captured cl output rather
:: than printing it only on failure: ANALYZE_FLAGS carries /analyze:WX-, so cl exits 0
:: with findings, and the failure-only convention would hide every one of them.
:ANALYZE
IF %verbose% EQU 1 (
    ECHO Analyze %~1
)
cl %CommonCompilerFlagsFinal% %ANALYZE_FLAGS% %ANALYZE_SUPPRESS% %~2 ^
%ANALYZE_EXTERNAL% %ANALYZE_INCLUDES% ^
/Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_OBJ%\%~n1_analyze.pdb /c ^
%DIR_REPO%\%~1 > "%TEMP_OUT%"
SET "ANALYZE_RC=!ERRORLEVEL!"
TYPE "%TEMP_OUT%"
DEL "%TEMP_OUT%"
IF NOT "!ANALYZE_RC!"=="0" (
    ECHO failed to analyze %~1
)
EXIT /B !ANALYZE_RC!

:ERROR
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
