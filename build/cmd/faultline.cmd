@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Unity build of the FaultLine driver. Builds faultline_tests.dll and
:: faultline.exe as unity translation units, linking the prebuilt sqlite3.obj /
:: cwalk.obj and sharing faultline_test_data.dll produced by faultline_fixtures.cmd.
:: Run faultline_fixtures.cmd first (all.cmd does this automatically).
:: Timing (faultline.ctm) covers only the unity FaultLine compiles.

SET PROJECT_NAME="Faultline"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
SET DIR_LOCAL=%DIR_CMD%\local
CALL %DIR_CMD%\options.cmd %*

CALL %DIR_CMD%\setup.cmd %*

:: cl output capture, named per script inside this configuration's obj directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%~n0_cl_out.tmp"

:: The nested fixtures build must not repeat the clean that setup.cmd already
:: performed, and it must write into the same output tree this script links
:: from: setup.cmd derives target\<vs>\<platform>\<buildtype> from the
:: option flags, and a nested call starts them all at zero, so without the
:: options forwarded it would default to debug/x64/newest-VS and put its
:: objects in the wrong target directory. Forward the options but strip the
:: clean/test verbs (test maps to build), the same filtering all.cmd uses.
set "args="
for %%A in (%*) do (
    if /I not "%%~A"=="test" if /I not "%%~A"=="clean" if /I not "%%~A"=="cleanall" if /I not "%%~A"=="cleanplat" (
        set "args=!args! %%~A"
    )
    if /I "%%~A"=="test" (
        set "args=!args! build"
    )
)

:: After 'clean' the shared fixtures are gone; rebuild them (untimed) so the
:: timed compile below can link sqlite3.obj / cwalk.obj and the test step can
:: load faultline_test_data.dll. all.cmd builds fixtures up front, so this is a
:: no-op there and on incremental rebuilds.
IF %build% EQU 1 IF NOT EXIST %DIR_OUT_OBJ%\sqlite3.obj (
    CALL %DIR_CMD%\faultline_fixtures.cmd !args!
    IF ERRORLEVEL 1 GOTO :ERROR
)

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\faultline.ctm
)

:: Build sources as C code by default. If cxx is 1, then build sources as C++.
SET "FL_FLAGS=%CommonCompilerFlagsFinal%"
IF %cxx% EQU 1 SET "FL_FLAGS=%CommonCompilerFlagsFinalCXX%"

:: /TP treats every file cl sees as a source file, .obj included -- it is not
:: just a compiler-frontend default, it applies to the whole command line, so a
:: prebuilt object listed ahead of /link gets fed through the C++ front end as
:: text under cxx and fails with binary-garbage syntax errors. Passing them as
:: /link arguments instead sidesteps that; it is also correct for the plain C
:: build, so nothing here is conditioned on cxx.

:: Build the project
IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %PROJECT_NAME% test suite ^(unity^): faultline_tests.dll
    )
    cl %FL_FLAGS% /wd4456 /wd4200 /wd4115 ^
    /I"%DIR_INCLUDE%" /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    /DDLL_BUILD ^
    %DIR_REPO%\src\faultline_tests_unity.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\faultline_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% /LIBPATH:%DIR_OUT_LIB% ^
    %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj ^
    /OUT:%DIR_OUT_BIN%\faultline_tests.dll /IMPLIB:%DIR_OUT_LIB%\faultline_tests.lib > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %PROJECT_NAME% test suite ^(unity^)
        GOTO :ERROR
    )
    del "%TEMP_OUT%"

    IF %verbose% EQU 1 (
        ECHO.
        ECHO Build the Faultline Test Program ^(unity^): faultline.exe
    )
    cl %FL_FLAGS% /wd4200 /wd4115 /wd4456 /DFL_PLATFORM_BUILD ^
    /I%DIR_INCLUDE% /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    /I%DIR_REPO%\src %DIR_REPO%\app\faultline\main_unity_windows.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\faultline.pdb /Fe:%DIR_OUT_BIN%\faultline.exe /link ^
    %CommonLinkerFlagsFinal% %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj ^
    /ENTRY:mainCRTStartup > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %PROJECT_NAME% Test Program ^(unity^)
        GOTO :ERROR
    )
    del "%TEMP_OUT%"
)
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME% unit tests
        ECHO.
    )
    pushd %DIR_OUT_BIN%
    .\faultline.exe run faultline_tests.dll
    .\faultline.exe show results --limit 1
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
