@ECHO OFF
SETLOCAL

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the MSVC build scripts package into dist\build_scripts\.
::
:: The package ships the shared build scripts a consumer repository builds its
:: suites with: options.cmd, shell.cmd, setup.cmd, config.cmd and
:: build_test_dll.cmd, plus project_template.cmd, which the consumer copies to
:: project.cmd and edits. Import it into the consumer's repository root, since
:: setup.cmd locates the repository two directories above itself.
::
:: What this package contains lives in build\dist\packages.psd1.
::
:: Usage:
::   build\cmd\build_scripts_dist.cmd [clean]
::
::   clean   - remove dist\build_scripts\ entirely
::
:: Output layout:
::   dist\build_scripts\build\cmd\  - build scripts (import with -Into <repo>)

SET PROJECT_NAME=MSVC Build Scripts Distribution
TITLE %PROJECT_NAME%

SET FL_CLEAN=
IF /I "%~1"=="clean" SET FL_CLEAN=-Clean
IF NOT "%~1"=="" IF NOT DEFINED FL_CLEAN (
    ECHO BUILD_SCRIPTS_DIST.CMD ERROR: unknown option "%~1" -- expected "clean" or no option. 1>&2
    EXIT /B 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\dist\fl_dist.ps1" -Package build_scripts %FL_CLEAN%
EXIT /B %ERRORLEVEL%
