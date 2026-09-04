@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Compile every test-suite translation unit as C++ and report which ones succeed.
:: A suite may be built either dialect, so each has to compile both ways; a suite that
:: does not is one nobody can build with cxx. A driver, host, library or example is
:: always C and is not measured here.
::
:: A suite unity unit pulls in the library sources it exercises, so between them these
:: units cover most of the tree even though nothing driver-side is listed.
::
:: Nothing links and no object is kept. A run answers one question -- how much is
:: left -- and changes no build output, so it is safe to run at any point.
::
:: Add a suite stem to SUITE_TUS when a new suite script is added.
::
:: Per-unit cl output lands in <obj>\cpp_probe\<stem>.txt so a failing unit can be
:: read without rerunning. The first error in each file is the one worth reading:
:: when many units report the same header, one fix there clears all of them. Pass
:: verbose to list every error of every failing unit instead of only the first.

SET PROJECT_NAME="FaultLine C++ Probe"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*

CALL %DIR_CMD%\setup.cmd %*
:: No supported Visual Studio for a requested build; setup.cmd said why.
IF ERRORLEVEL 1 EXIT /B 1

:: Nothing to probe when the caller asked only for a clean. config.cmd runs only when
:: build is set, so the compiler flags would be empty here as well.
IF %build% EQU 0 GOTO :SUCCESS

SET "DIR_PROBE=%DIR_OUT_OBJ%\cpp_probe"
IF NOT EXIST "%DIR_PROBE%" MKDIR "%DIR_PROBE%"

:: First-party include paths, as faultline_analyze.cmd sets them. The third-party
:: directories are named only by /external:I below, which searches them as well.
SET PROBE_INCLUDES=/I%DIR_INCLUDE% /I%DIR_REPO%\src

:: Header directories whose diagnostics are not ours to fix.
SET PROBE_EXTERNAL=/external:W0 /external:env:INCLUDE ^
    /external:I "%DIR_THIRD_PARTY%" /external:I "%DIR_THIRD_PARTY%\cwalk\include" ^
    /external:I "%DIR_THIRD_PARTY%\fnv"

:: The union of the /wd sets the build lines carry, applied to every unit.
SET PROBE_SUPPRESS=/wd4456 /wd4200 /wd4115 /wd4127

:: /std:c17 and /experimental:c11atomics are C-only: cl rejects the first outright
:: when /std:c++20 is also present, and the second selects a C header. Drop both and
:: name the C++ dialect instead.
::
:: /std:c++20 rather than c++17 because designated initializers are a C++20 feature
:: and this codebase uses them throughout.
::
:: /WX- so a unit reports every diagnostic it has rather than stopping at the first
:: one /WX would promote to an error. The count per unit is the measurement.
SET "PROBE_FLAGS=%CommonCompilerFlagsFinal:/std:c17=%"
SET "PROBE_FLAGS=%PROBE_FLAGS:/experimental:c11atomics=%"
SET "PROBE_FLAGS=%PROBE_FLAGS% /TP /std:c++20 /WX-"

SET /A PROBE_PASS=0
SET /A PROBE_FAIL=0

:: Test suites only. A suite is the one kind of binary built either dialect, so it is
:: the one kind that has to compile both ways. Drivers, hosts, libraries and examples
:: are always C and are not probed.
CALL :PROBE src\faultline_tests_unity.c "/DDLL_BUILD"

:: Suites whose build scripts do not go through build_test_dll.cmd.
CALL :PROBE src\but_tests.c "/DDLL_BUILD"
CALL :PROBE src\index_generic_tests.c "/DDLL_BUILD"
CALL :PROBE src\index_windows_tests.c "/DDLL_BUILD"
CALL :PROBE src\malloc_cleanup_config_tests.c "/DDLL_BUILD"

:: The per-component suites built by build_test_dll.cmd. Bare stems because they
:: differ in nothing this pass cares about: each is one src\<stem>.c, /DDLL_BUILD.
SET SUITE_TUS=arena_tests arena_pool_tests bits_tests buffer_tests chunk_tests ^
    command_tests digital_search_tree_tests dlist_tests fault_injector_tests ^
    fl_assert_tests fl_exception_tests fl_log_service_tests flp_file_service_tests ^
    flp_memory_service_tests flp_stream_service_tests fnv_tests math_tests ^
    region_tests set_tests timer_tests

FOR %%T IN (%SUITE_TUS%) DO CALL :PROBE src\%%T.c "/DDLL_BUILD"

ECHO.
SET /A PROBE_TOTAL=!PROBE_PASS!+!PROBE_FAIL!
ECHO %PROJECT_NAME%: !PROBE_PASS! of !PROBE_TOTAL! units compile as C++, !PROBE_FAIL! do not.
ECHO Per-unit cl output: %DIR_PROBE%
GOTO :SUCCESS

:: Probe one translation unit. %1 is its path below the repository root, %2 its /D set.
::
:: Compiles to /Fo in the probe directory so a partial object never lands where the
:: real build would find it, and /c so nothing links.
:PROBE
SET "STEM=%~n1"
cl %PROBE_FLAGS% %PROBE_SUPPRESS% %~2 %PROBE_EXTERNAL% %PROBE_INCLUDES% ^
/Fo:%DIR_PROBE%\ /Fd:%DIR_PROBE%\%STEM%_probe.pdb /c ^
%DIR_REPO%\%~1 > "%DIR_PROBE%\%STEM%.txt" 2>&1
IF "!ERRORLEVEL!"=="0" GOTO :PROBE_OK

SET /A PROBE_FAIL+=1
SET "PROBE_FIRST="
SET /A PROBE_ERRS=0
:: findstr by full path. A bare FIND or FINDSTR is resolved through PATH, and a
:: shell that puts a Unix toolchain ahead of System32 turns FIND into a
:: filesystem walk that never returns.
:: The loop is not inside a parenthesised block on purpose: such a block is
:: parsed twice, which eats one level of escaping.
FOR /F "usebackq delims=" %%L IN (`%SystemRoot%\System32\findstr.exe /C:"error C" "%DIR_PROBE%\%STEM%.txt"`) DO CALL :PROBE_TALLY "%%L"
ECHO   FAIL %~1 -- !PROBE_ERRS! errors
IF %verbose% EQU 1 CALL :PROBE_LIST "%DIR_PROBE%\%STEM%.txt"
IF %verbose% NEQ 1 IF DEFINED PROBE_FIRST ECHO          first: !PROBE_FIRST!
EXIT /B 0

:PROBE_OK
SET /A PROBE_PASS+=1
ECHO   pass %~1
EXIT /B 0

:: Count one diagnostic line and keep the first. The first is the one worth
:: reading: it names the header or source the unit stopped on, and when many
:: units name the same one, a single fix there clears all of them.
:PROBE_TALLY
SET /A PROBE_ERRS+=1
IF NOT DEFINED PROBE_FIRST SET "PROBE_FIRST=%~1"
EXIT /B 0

:: List every diagnostic in one unit's capture, for a verbose run. A second pass
:: over the same file rather than a buffer filled by the first: the count line has
:: to print before the list it introduces, and the count is not known until that
:: first pass ends.
:PROBE_LIST
FOR /F "usebackq delims=" %%L IN (`%SystemRoot%\System32\findstr.exe /C:"error C" %1`) DO CALL :PROBE_ECHO "%%L"
EXIT /B 0

:: Echo one diagnostic. The text reaches ECHO through a variable and delayed
:: expansion because cl writes < and > in type names: delayed expansion runs after
:: the line is parsed for redirection, so those characters print instead of
:: opening a file.
:PROBE_ECHO
SET "PROBE_LINE=%~1"
ECHO          !PROBE_LINE!
EXIT /B 0

:SUCCESS
ENDLOCAL
EXIT /B 0
