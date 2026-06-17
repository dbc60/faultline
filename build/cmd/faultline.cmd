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
    ctime.exe -begin metrics\vs\faultline.ctm
)

:: Build the project
IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %PROJECT_NAME% test suite ^(unity^): faultline_tests.dll
    )
    cl %CommonCompilerFlagsFinal% /wd4456 /wd4200 /wd4115 ^
    /I"%DIR_INCLUDE%" /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    /DDLL_BUILD ^
    %DIR_REPO%\src\faultline_tests_unity.c ^
    %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\faultline_tests.pdb ^
    /LD /link %CommonLinkerFlagsFinal% /LIBPATH:%DIR_OUT_LIB% ^
    /OUT:%DIR_OUT_BIN%\faultline_tests.dll /IMPLIB:%DIR_OUT_LIB%\faultline_tests.lib > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% test suite ^(unity^)
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    IF %verbose% EQU 1 (
        ECHO.
        ECHO Build the Faultline Test Program ^(unity^): faultline.exe
    )
    cl %CommonCompilerFlagsFinal% /wd4200 /wd4115 /wd4456 /DFL_BUILD_DRIVER ^
    /I%DIR_INCLUDE% /I"%DIR_THIRD_PARTY%" /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    /I%DIR_REPO%\src %DIR_REPO%\app\faultline\main_unity_windows.c ^
    %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\faultline.pdb /Fe:%DIR_OUT_BIN%\faultline.exe /link ^
    %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP%\cl_out.tmp"
    if errorlevel 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo failed to build the %PROJECT_NAME% Test Program ^(unity^)
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"
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
