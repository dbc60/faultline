@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Split-architecture build of win32_faultline.exe: compiles the Win32 platform
:: layer (win32_faultline_unity.c) and links it against faultline_core.lib plus
:: the prebuilt sqlite3.obj / cwalk.obj. This is the platform/app counterpart to
:: the monolithic faultline.cmd, which is left intact for the current
:: architecture.
::
:: Pipeline:
::   1. faultline_fixtures.cmd  -> sqlite3.obj / cwalk.obj / faultline_test_data.dll
::   2. faultline_core.cmd      -> faultline_core.lib (the OS-free app layer)
::   3. faultline.cmd           -> faultline_tests.dll (only when testing and missing)
::   4. this script             -> win32_faultline.exe (the platform layer)

SET PROJECT_NAME="Faultline Split"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
SET DIR_LOCAL=%DIR_CMD%\local
CALL %DIR_CMD%\options.cmd %*
CALL %DIR_CMD%\setup.cmd %*

:: cl output capture, named per script inside this configuration's obj directory
SET "TEMP_OUT=%DIR_OUT_OBJ%\%~n0_cl_out.tmp"

:: The nested sub-builds must not repeat the clean that setup.cmd already
:: performed, and they must write into the same output tree this script links
:: from: setup.cmd derives target\<vs>\<platform>\<buildtype> from the
:: option flags, and a nested call starts them all at zero, so without the
:: options forwarded they would default to debug/x64/newest-VS and put their
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
:: timed compile below can link sqlite3.obj / cwalk.obj. all.cmd builds fixtures
:: up front, so this is a no-op there and on incremental rebuilds.
IF %build% EQU 1 IF NOT EXIST %DIR_OUT_OBJ%\sqlite3.obj (
    CALL %DIR_CMD%\faultline_fixtures.cmd !args!
    IF ERRORLEVEL 1 GOTO :ERROR
)

:: Build the OS-free application layer first.
IF %build% EQU 1 (
    CALL %DIR_CMD%\faultline_core.cmd !args!
    IF ERRORLEVEL 1 GOTO :ERROR
)

:: The test step below runs faultline_tests.dll; after a clean it is gone.
:: faultline.cmd rebuilds it (and, alongside, the monolithic faultline.exe).
IF %test% EQU 1 IF NOT EXIST %DIR_OUT_BIN%\faultline_tests.dll (
    CALL %DIR_CMD%\faultline.cmd !args!
    IF ERRORLEVEL 1 GOTO :ERROR
)

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\faultline_split.ctm
)

:: Always C. cxx selects the dialect of a test suite, and this is not one.
SET "SPLIT_FLAGS=%CommonCompilerFlagsFinal%"

:: Build the platform layer and link the core lib + prebuilt objs.
IF %build% EQU 1 (
    IF %verbose% EQU 1 (
        ECHO.
        ECHO Build the %PROJECT_NAME% platform layer ^(unity^): win32_faultline.exe
    )
    cl %SPLIT_FLAGS% /wd4200 /wd4115 /wd4456 /DFL_PLATFORM_BUILD ^
    /DFL_EMBEDDED ^
    /I%DIR_INCLUDE% /I%DIR_REPO%\src /I"%DIR_THIRD_PARTY%" ^
    /I"%DIR_THIRD_PARTY%\cwalk\include" ^
    %DIR_REPO%\app\faultline\win32_faultline_unity.c /Fo:%DIR_OUT_OBJ%\ ^
    /Fd:%DIR_OUT_BIN%\win32_faultline.pdb /Fe:%DIR_OUT_BIN%\win32_faultline.exe /link ^
    %CommonLinkerFlagsFinal% %DIR_OUT_LIB%\faultline_core.lib ^
    %DIR_OUT_OBJ%\sqlite3.obj %DIR_OUT_OBJ%\cwalk.obj /ENTRY:mainCRTStartup > "%TEMP_OUT%"
    if errorlevel 1 (
        type "%TEMP_OUT%"
        del "%TEMP_OUT%"
        echo failed to build the %PROJECT_NAME% platform layer ^(unity^)
        GOTO :ERROR
    )
    del "%TEMP_OUT%"
)

if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline_split.ctm %errorlevel%
)

if %test% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Run %PROJECT_NAME% unit tests
        ECHO.
    )
    pushd %DIR_OUT_BIN%
    .\win32_faultline.exe run faultline_tests.dll
    .\win32_faultline.exe show results --limit 1
    popd
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline_split.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
