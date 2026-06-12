@ECHO off
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.

:: Locate and activate a Visual Studio C++ build environment.
::
:: Visual Studio is found via vswhere.exe -- which ships at a fixed location with
:: VS 2017 and later -- rather than by probing hardcoded install directories. The
:: build therefore keeps working across VS editions and versions, including new
:: versions that appear on updated CI runner images (e.g. VS 2026 on the
:: windows-latest runner). The located edition's vcvars{64,32}.bat is called to
:: put cl on PATH and export the SDK environment.
::
:: This script is called once per build script (all.cmd plus each component), so
:: it has two paths:
::   * Not yet in a dev environment (VSINSTALLDIR empty): locate VS with vswhere
::     and call vcvars to activate it.
::   * Already activated by a parent (VSINSTALLDIR set, e.g. a component called
::     from all.cmd): skip activation, but STILL derive VSSOLUTION so every build
::     script agrees on the target\ subfolder. (Skipping it here makes setup.cmd
::     fall back to its default and the parent/child write to different trees.)
::
:: On success this sets, in the caller's environment:
::   VSSOLUTION   - vsYYYY (e.g. vs2022, vs2026); used as the target\ subfolder
::   VSINSTALLDIR plus the rest of the VS/SDK variables exported by vcvars

:: Already inside a developer environment? Reuse it; just take the active version
:: so VSSOLUTION can still be derived below. vcvars sets VisualStudioVersion.
IF NOT "%VSINSTALLDIR%"=="" (
    SET "VS_VER=!VisualStudioVersion!"
    GOTO :have_version
)

SET "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
IF NOT EXIST "%VSWHERE%" (
    ECHO Visual Studio locator vswhere.exe not found at:
    ECHO   "%VSWHERE%"
    ECHO Visual Studio 2017 or later with the C++ toolset is required.
    GOTO :EOF
)

:: When exactly one VS year is requested, constrain vswhere to that major
:: version; otherwise take the latest installed VS. The ranges are left UNquoted:
:: the SET "..." protects the ')' on this line, and below VS_RANGE is expanded
:: with delayed expansion inside the FOR /F (so the ')' is absent at parse time
:: and merely a literal argument char -- with no matching '(' -- at run time).
:: Quoting the range here instead unbalances the quotes around "%VSWHERE%" when
:: the FOR /F command is executed, dropping vswhere's leading quote.
::   VS2017 = 15.x, VS2019 = 16.x, VS2022 = 17.x, VS2026 = 18.x
SET "VS_RANGE="
IF %vs2017% EQU 1 IF %vs2019% EQU 0 IF %vs2022% EQU 0 SET "VS_RANGE=-version [15.0,16.0)"
IF %vs2019% EQU 1 IF %vs2017% EQU 0 IF %vs2022% EQU 0 SET "VS_RANGE=-version [16.0,17.0)"
IF %vs2022% EQU 1 IF %vs2017% EQU 0 IF %vs2019% EQU 0 SET "VS_RANGE=-version [17.0,18.0)"

:: Only consider installs that actually carry the C++ x86/x64 toolset, and allow
:: any product (Community/Professional/Enterprise/BuildTools).
SET "VS_REQUIRE=-products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64"

:: Query the install path and the installation version (e.g. 17.14.x, 18.6.x).
CALL :VSWHERE_PROP installationPath VSINSTALL
CALL :VSWHERE_PROP installationVersion VS_VER

IF "%VSINSTALL%"=="" (
    ECHO No Visual Studio installation with the C++ x86/x64 toolset was found.
    ECHO   vswhere : "%VSWHERE%"
    GOTO :EOF
)

:have_version
:: Map the MSVC major version to its marketing year so the target\ subfolder
:: keeps the familiar vsYYYY name (vswhere's catalog_productLineVersion is
:: inconsistent -- "2022" for VS2022 but "18" for VS2026 -- so derive it here).
:: Unknown future majors fall through as vs<major>, which is still stable.
FOR /f "tokens=1 delims=." %%V IN ("%VS_VER%") DO SET "VS_MAJOR=%%V"
SET "VS_YEAR=%VS_MAJOR%"
IF "%VS_MAJOR%"=="15" SET "VS_YEAR=2017"
IF "%VS_MAJOR%"=="16" SET "VS_YEAR=2019"
IF "%VS_MAJOR%"=="17" SET "VS_YEAR=2022"
IF "%VS_MAJOR%"=="18" SET "VS_YEAR=2026"
IF NOT "%VS_YEAR%"=="" SET "VSSOLUTION=vs%VS_YEAR%"

:: Already in a developer environment: VSSOLUTION is set, nothing to activate.
IF NOT "%VSINSTALLDIR%"=="" GOTO :export

:: VS 2026's vcvars invokes vswhere by name, so put the VS Installer directory
:: (which contains vswhere.exe) on PATH before activating, or it prints a benign
:: "'vswhere.exe' is not recognized" / "system cannot find the file" warning.
FOR %%I IN ("%VSWHERE%") DO SET "PATH=%%~dpI;%PATH%"

:: Activate the toolchain for the target architecture.
IF %x64% EQU 1 (
    CALL "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
) ELSE IF %x86% EQU 1 (
    CALL "%VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat"
)

IF "%VSINSTALLDIR%" == "" GOTO badenv

:export
:: Set some variables in the shell
ENDLOCAL & (
    REM Project environment variables
    SET "VSSOLUTION=%VSSOLUTION%"

    REM Visual Studio environment variables
    SET "VSINSTALLDIR=%VSINSTALLDIR%"
    SET "PATH=%PATH%"
    SET "PLATFORM=%PLATFORM%"
    SET "INCLUDE=%INCLUDE%"
    SET "LIB=%LIB%"
    SET "LIBPATH=%LIBPATH%"
    SET "UCRTVersion=%UCRTVersion%"
    SET "UniversalCRTSdkDir=%UniversalCRTSdkDir%"
    SET "VCIDEInstallDir=%VCIDEInstallDir%"
    SET "VCINSTALLDIR=%VCINSTALLDIR%"
    SET "VCToolsInstallDir=%VCToolsInstallDir%"
    SET "VCToolsRedistDir=%VCToolsRedistDir%"
    SET "VCToolsVersion=%VCToolsVersion%"
    SET "VisualStudioVersion=%VisualStudioVersion%"
    SET "VS150COMNTOOLS=%VS150COMNTOOLS%"
    SET "VS160COMNTOOLS=%VS160COMNTOOLS%"
    SET "WindowsLibPath=%WindowsLibPath%"
    SET "WindowsSdkBinPath=%WindowsSdkBinPath%"
    SET "WindowsSdkDir=%WindowsSdkDir%"
    SET "WindowsSDKLibVersion=%WindowsSDKLibVersion%"
    SET "WindowsSdkVerBinPath=%WindowsSdkVerBinPath%"
    SET "WindowsSDKVersion=%WindowsSDKVersion%"
    SET "DevEnvDir=%DevEnvDir%"
    SET "ExtensionSdkDir=%ExtensionSdkDir%"
    SET "Framework40Version=%Framework40Version%"
    SET "FrameworkDir=%FrameworkDir%"
    SET "FrameworkDir64=%FrameworkDir64%"
    SET "FrameworkDir32=%FrameworkDir32%"
    SET "FrameworkVersion=%FrameworkVersion%"
    SET "FrameworkVersion64=%FrameworkVersion64%"
    SET "FrameworkVersion32=%FrameworkVersion32%"
)
GOTO :EOF

:: -----------------------------------------------------------------------
:: :VSWHERE_PROP <vswhere-property> <out-var>
:: Run vswhere for the selected version range and toolset, capturing a single
:: property into <out-var>. VS_RANGE is expanded with delayed expansion so its
:: ')' cannot disturb the FOR /F parenthesis matching.
:: -----------------------------------------------------------------------
:VSWHERE_PROP
SET "%~2="
FOR /f "usebackq tokens=* delims=" %%I IN (`"%VSWHERE%" -latest !VS_RANGE! !VS_REQUIRE! -property %~1`) DO SET "%~2=%%I"
EXIT /B 0

:badenv
ECHO VSINSTALLDIR is not defined after calling vcvars.
ECHO The located Visual Studio install may be missing the C++ toolset:
ECHO   "%VSINSTALL%"
