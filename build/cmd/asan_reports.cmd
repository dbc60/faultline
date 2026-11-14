@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: asan_reports.cmd - Shared helper for the asan build option. Called by all.cmd and
::   build_test_dll.cmd; not intended to be invoked directly.
::
:: ASan ends the process at its first report and, by default, writes the report to
:: stderr, where a suite that redirects stderr can discard it. ASAN_OPTIONS log_path
:: writes it to <REPORT_STEM>.<pid> in the working directory instead, so the scripts
:: can find it and print it after the run.
::
:: Usage:
::   CALL asan_reports.cmd env                    Append log_path to ASAN_OPTIONS in the
::                                                caller's environment. A log_path the
::                                                caller set is overridden, because the
::                                                checks below read only REPORT_STEM.
::   CALL asan_reports.cmd clear  <dir>           Delete <dir>\<REPORT_STEM>.*
::   CALL asan_reports.cmd check  <dir>           Print each <dir>\<REPORT_STEM>.* and
::                                                exit 1; exit 0 when there is none
::   CALL asan_reports.cmd canary <dir> <driver>  Run asan_canary_tests.dll from <dir>
::                                                and exit 1 unless ASan reported the
::                                                canary's heap overflow
::
::   +-----------+  run   +----------------+  log_path  +----------------------+
::   | all.cmd   | -----> | faultline.exe  | ---------> | test\asan_report.pid |
::   +-----------+        +----------------+            +----------------------+
::         |                                                       |
::         +-------------- asan_reports.cmd check <----------------+

SET "REPORT_STEM=asan_report"
SET "CANARY_DLL=asan_canary_tests.dll"
SET "CANARY_SRC=asan_canary_tests.c"
SET "CANARY_KIND=heap-buffer-overflow"
SET "CANARY_OUT=asan_canary.out"

SET "VERB=%~1"
SET "DIR=%~2"

IF /I "%VERB%"=="env" GOTO :ENV
IF /I "%VERB%"=="clear" GOTO :CLEAR
IF /I "%VERB%"=="check" GOTO :CHECK
IF /I "%VERB%"=="canary" GOTO :CANARY
ECHO ASAN_REPORTS.CMD ERROR: unknown verb "%VERB%" 1>&2
EXIT /B 1

:: A later option overrides an earlier one, so appending wins over a caller's log_path.
:ENV
SET "ASAN_OPTIONS=%ASAN_OPTIONS% log_path=%REPORT_STEM%"
ENDLOCAL & SET "ASAN_OPTIONS=%ASAN_OPTIONS%"
EXIT /B 0

:CLEAR
IF EXIST "%DIR%\%REPORT_STEM%.*" DEL /Q "%DIR%\%REPORT_STEM%.*"
EXIT /B 0

:CHECK
IF NOT EXIST "%DIR%\%REPORT_STEM%.*" EXIT /B 0
ECHO ASAN ERROR: AddressSanitizer report^(s^) follow 1>&2
FOR %%F IN ("%DIR%\%REPORT_STEM%.*") DO (
    ECHO ---- %%~nxF 1>&2
    TYPE "%%F" 1>&2
)
EXIT /B 1

:: The canary passes only when all three hold: the driver exits non-zero, a report
:: exists, and the report names the canary's overflow in the canary's source. --no-db
:: keeps the deliberate failure out of the results database.
:CANARY
SET "DRIVER=%~3"
SET "ASAN_OPTIONS=%ASAN_OPTIONS% log_path=%REPORT_STEM%"
CALL :CLEAR

PUSHD "%DIR%"
"%DRIVER%" run --no-db %CANARY_DLL% > "%CANARY_OUT%" 2>&1
SET "RC=!errorlevel!"
POPD

IF "!RC!"=="0" (
    ECHO ASAN CANARY ERROR: the driver exited 0, so the canary's overflow went 1>&2
    ECHO undetected. The binaries are not instrumented or the ASan runtime is not loaded. 1>&2
    GOTO :CANARY_FAIL
)

IF NOT EXIST "%DIR%\%REPORT_STEM%.*" (
    ECHO ASAN CANARY ERROR: the driver exited !RC! but wrote no %REPORT_STEM%.* file. 1>&2
    GOTO :CANARY_FAIL
)

FINDSTR /C:"%CANARY_KIND%" "%DIR%\%REPORT_STEM%.*" > NUL
IF ERRORLEVEL 1 GOTO :CANARY_WRONG_REPORT
FINDSTR /C:"%CANARY_SRC%" "%DIR%\%REPORT_STEM%.*" > NUL
IF ERRORLEVEL 1 GOTO :CANARY_WRONG_REPORT

CALL :CLEAR
DEL /Q "%DIR%\%CANARY_OUT%"
ECHO ASan canary passed: %CANARY_KIND% in %CANARY_SRC% reported as expected
EXIT /B 0

:CANARY_WRONG_REPORT
ECHO ASAN CANARY ERROR: the report does not name a %CANARY_KIND% in %CANARY_SRC%. 1>&2
CALL :CHECK

:CANARY_FAIL
ECHO ---- driver output 1>&2
TYPE "%DIR%\%CANARY_OUT%" 1>&2
DEL /Q "%DIR%\%CANARY_OUT%"
CALL :CLEAR
EXIT /B 1
