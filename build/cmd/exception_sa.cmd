@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Build and optionally run the standalone exception test executable.
::
:: Usage:
::   build\cmd\exception_sa.cmd [build options...]
::
:: Options (forwarded to options.cmd / setup.cmd):
::   debug, release, clean, cleanall, test, verbose, x64, x86, vs2022, ...
::
:: The executable is built at:
::   target\<vs>\<platform>\<build_type>\bin\exception_sa_test.exe
:: and copied to test\ for easy execution.

SET PROJECT_NAME=Standalone Exception
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%

CALL %DIR_CMD%\options.cmd %*
CALL %DIR_CMD%\setup.cmd %*

IF %build% EQU 1 (
    IF %verbose% EQU 1 ECHO Build the %PROJECT_NAME% test executable

    cl %CommonCompilerFlagsFinal% /DFL_EMBEDDED /wd4456 ^
        /I"%DIR_REPO%\standalone\exception\include" ^
        /I"%DIR_INCLUDE%" ^
        "%DIR_REPO%\src\fl_exception_service.c" ^
        "%DIR_REPO%\src\fla_exception_service.c" ^
        "%DIR_REPO%\standalone\exception\src\exception_sa_test.c" ^
        /Fo:"%DIR_OUT_OBJ%\\" ^
        /Fd:"%DIR_OUT_BIN%\exception_sa_test.pdb" ^
        /Fe:"%DIR_OUT_BIN%\exception_sa_test.exe" ^
        /link %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%TEMP%\cl_out.tmp"
    IF ERRORLEVEL 1 (
        TYPE "%TEMP%\cl_out.tmp"
        DEL "%TEMP%\cl_out.tmp"
        ECHO Failed to build %PROJECT_NAME%
        GOTO :ERROR
    )
    DEL "%TEMP%\cl_out.tmp"

    IF NOT EXIST "%DIR_REPO%\test" MD "%DIR_REPO%\test"
    COPY /Y "%DIR_OUT_BIN%\exception_sa_test.exe" "%DIR_REPO%\test\exception_sa_test.exe" > NUL
    IF %verbose% EQU 1 ECHO Copied exception_sa_test.exe to test\
)

IF %test% EQU 1 (
    IF %verbose% EQU 1 ECHO Run %PROJECT_NAME%
    "%DIR_REPO%\test\exception_sa_test.exe"
    IF ERRORLEVEL 1 GOTO :ERROR
)

GOTO :SUCCESS

:ERROR
ENDLOCAL
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
