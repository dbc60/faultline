<#
.SYNOPSIS
    Installs the FaultLine SDK to a prefix directory.

.DESCRIPTION
    Copies public headers, the faultline.lib static library, and faultline.exe
    to a conventional prefix directory layout:

        <Prefix>\FaultLine\
            include\
                faultline\          all public headers (under faultline\ subdirectory)
            lib\
                faultline.lib       static library that test suite DLLs link against
                faultline.pdb       (debug builds only, if present)
                cmake\Faultline\
                    FaultlineConfig.cmake   CMake find_package() support
            bin\
                faultline.exe       test driver
                faultline.pdb       (debug builds only, if present)

    Test suite authors link against faultline.lib and include headers from
    include\. Compile test suites with /DDLL_BUILD. Run them with faultline.exe.

    CMake users: after installation, set CMAKE_PREFIX_PATH to <Prefix>\FaultLine
    and call find_package(Faultline CONFIG REQUIRED). This creates the imported
    target Faultline::faultline.

    The script is idempotent: re-running it with the same arguments updates any
    changed files in place. Use -Force to overwrite without prompting.

.PARAMETER Prefix
    Root of the installation tree. The FaultLine subdirectory is created inside
    it. Example: C:\libs\faultline

.PARAMETER RepoRoot
    Root of the FaultLine source repository. Defaults to two directories above
    this script (build\ps\ -> build\ -> repo root).

.PARAMETER Toolchain
    Build toolchain whose output to install. Accepted values: vs2022, clang.
    Default: vs2022

.PARAMETER Platform
    Target architecture. Accepted values: x64, x86.
    Default: x64
    Note: the clang toolchain only supports x64.

.PARAMETER BuildType
    Build configuration to install. Accepted values: debug, release.
    Default: release

.PARAMETER Force
    Overwrite existing files without prompting.

.EXAMPLE
    .\install-sdk.ps1 -Prefix C:\libs\faultline

    Installs the vs2022 x64 release build to C:\libs\faultline\FaultLine\.

.EXAMPLE
    .\install-sdk.ps1 -Prefix C:\libs\faultline -Toolchain clang -BuildType debug -Force

    Installs the clang x64 debug build, overwriting any existing files.

.EXAMPLE
    .\install-sdk.ps1 -Prefix C:\libs\faultline -WhatIf

    Shows what would be copied without making any changes.
#>

[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'Low')]
param (
    [Parameter(Mandatory)]
    [string] $Prefix,

    [string] $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,

    [ValidateSet('vs2022', 'clang')]
    [string] $Toolchain = 'vs2022',

    [ValidateSet('x64', 'x86')]
    [string] $Platform = 'x64',

    [ValidateSet('debug', 'release')]
    [string] $BuildType = 'release',

    [switch] $Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Validate inputs
# ---------------------------------------------------------------------------

if ($Toolchain -eq 'clang' -and $Platform -eq 'x86') {
    Write-Error "The clang toolchain only supports x64. Use -Platform x64 or -Toolchain vs2022."
}

$BuildOut = Join-Path $RepoRoot "target\$Toolchain\$Platform\$BuildType"

if (-not (Test-Path $BuildOut)) {
    Write-Error "Build output directory not found: $BuildOut`nBuild the project first (build\cmd\all.cmd)."
}

$BinSource = Join-Path $BuildOut 'bin'
$LibSource  = Join-Path $BuildOut 'lib'

$ExeSource = Join-Path $BinSource 'faultline.exe'
if (-not (Test-Path $ExeSource)) {
    Write-Error "faultline.exe not found: $ExeSource`nBuild the project first (build\cmd\faultline.cmd or all.cmd)."
}

$FaultlineLibSource = Join-Path $LibSource 'faultline.lib'
if (-not (Test-Path $FaultlineLibSource)) {
    Write-Error "faultline.lib not found: $FaultlineLibSource`nRun build\cmd\faultline_lib.cmd (or all.cmd) first."
}

$IncludeSource = Join-Path $RepoRoot 'include'

# ---------------------------------------------------------------------------
# Destination layout
# ---------------------------------------------------------------------------

$DestRoot    = Join-Path $Prefix 'FaultLine'
$DestInclude = Join-Path $DestRoot 'include'
$DestLib     = Join-Path $DestRoot 'lib'
$DestBin     = Join-Path $DestRoot 'bin'
$DestCMake   = Join-Path $DestLib 'cmake\Faultline'

# ---------------------------------------------------------------------------
# Helper: copy a single file, creating the destination directory as needed
# ---------------------------------------------------------------------------

function Install-File {
    param (
        [string] $Src,
        [string] $Dst
    )

    $DstDir = Split-Path $Dst -Parent
    if (-not (Test-Path $DstDir)) {
        if ($PSCmdlet.ShouldProcess($DstDir, 'Create directory')) {
            New-Item -ItemType Directory -Path $DstDir -Force | Out-Null
        }
    }

    if ($PSCmdlet.ShouldProcess($Dst, 'Copy file')) {
        $CopyArgs = @{ Path = $Src; Destination = $Dst; ErrorAction = 'Stop' }
        if ($Force) { $CopyArgs['Force'] = $true }
        Copy-Item @CopyArgs
    }

    Write-Verbose "  $Src -> $Dst"
}

# ---------------------------------------------------------------------------
# Helper: write a text file (only writes if content differs)
# ---------------------------------------------------------------------------

function Install-TextFile {
    param (
        [string] $Dst,
        [string] $Content
    )

    $DstDir = Split-Path $Dst -Parent
    if (-not (Test-Path $DstDir)) {
        if ($PSCmdlet.ShouldProcess($DstDir, 'Create directory')) {
            New-Item -ItemType Directory -Path $DstDir -Force | Out-Null
        }
    }

    if ($PSCmdlet.ShouldProcess($Dst, 'Write file')) {
        $Existing = if (Test-Path $Dst) { Get-Content -Raw $Dst } else { $null }
        if ($Existing -ne $Content) {
            Set-Content -Path $Dst -Value $Content -Encoding UTF8 -NoNewline -ErrorAction Stop
        }
    }

    Write-Verbose "  (generated) -> $Dst"
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "FaultLine SDK install"
Write-Host "  Repo     : $RepoRoot"
Write-Host "  Toolchain: $Toolchain"
Write-Host "  Platform : $Platform"
Write-Host "  Build    : $BuildType"
Write-Host "  Dest     : $DestRoot"
Write-Host ""

# ---------------------------------------------------------------------------
# 1. Public headers (under faultline\ subdirectory)
# ---------------------------------------------------------------------------

Write-Host "Copying headers..."

$FaultlineInclude = Join-Path $IncludeSource 'faultline'
foreach ($Header in (Get-ChildItem -Path $FaultlineInclude -File)) {
    Install-File -Src $Header.FullName -Dst (Join-Path $DestInclude "faultline\$($Header.Name)")
}

# ---------------------------------------------------------------------------
# 3. Static library
# ---------------------------------------------------------------------------

Write-Host "Copying faultline.lib..."
Install-File -Src $FaultlineLibSource -Dst (Join-Path $DestLib 'faultline.lib')

$LibPdbSource = Join-Path $LibSource 'faultline.pdb'
if (Test-Path $LibPdbSource) {
    Write-Host "Copying faultline.pdb (lib)..."
    Install-File -Src $LibPdbSource -Dst (Join-Path $DestLib 'faultline.pdb')
}

# ---------------------------------------------------------------------------
# 4. CMake package config
#
#    Placed at lib\cmake\Faultline\FaultlineConfig.cmake so that setting
#    CMAKE_PREFIX_PATH to <Prefix>\FaultLine is sufficient for find_package().
#    Paths are relative to CMAKE_CURRENT_LIST_DIR so the tree is relocatable.
# ---------------------------------------------------------------------------

Write-Host "Generating FaultlineConfig.cmake..."

$CMakeConfig = @"
# FaultlineConfig.cmake
# Generated by install-sdk.ps1 -- do not edit by hand.
#
# Usage in CMakeLists.txt:
#   list(APPEND CMAKE_PREFIX_PATH "path/to/FaultLine")
#   find_package(Faultline CONFIG REQUIRED)
#   target_link_libraries(my_tests PRIVATE Faultline::faultline)

cmake_minimum_required(VERSION 3.15)

# Compute the install prefix relative to this file's location:
#   lib/cmake/Faultline/FaultlineConfig.cmake  ->  ../../..  ->  <DestRoot>
get_filename_component(_fl_prefix "`${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if (NOT TARGET Faultline::faultline)
    add_library(Faultline::faultline STATIC IMPORTED GLOBAL)
    set_target_properties(Faultline::faultline PROPERTIES
        IMPORTED_LOCATION             "`${_fl_prefix}/lib/faultline.lib"
        INTERFACE_INCLUDE_DIRECTORIES "`${_fl_prefix}/include"
        INTERFACE_COMPILE_DEFINITIONS "DLL_BUILD"
    )
endif()

set(FAULTLINE_FOUND       TRUE)
set(FAULTLINE_INCLUDE_DIR  "`${_fl_prefix}/include")
set(FAULTLINE_LIBRARY      "`${_fl_prefix}/lib/faultline.lib")
set(FAULTLINE_EXECUTABLE   "`${_fl_prefix}/bin/faultline.exe")

unset(_fl_prefix)
"@

Install-TextFile -Dst (Join-Path $DestCMake 'FaultlineConfig.cmake') -Content $CMakeConfig

# ---------------------------------------------------------------------------
# 5. Executable
# ---------------------------------------------------------------------------

Write-Host "Copying faultline.exe..."
Install-File -Src $ExeSource -Dst (Join-Path $DestBin 'faultline.exe')

$ExePdbSource = Join-Path $BinSource 'faultline.pdb'
if (Test-Path $ExePdbSource) {
    Write-Host "Copying faultline.pdb (exe)..."
    Install-File -Src $ExePdbSource -Dst (Join-Path $DestBin 'faultline.pdb')
}

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "Done. Installed to: $DestRoot"
Write-Host ""
$DestIncludeRoot = Join-Path $DestRoot 'include'
Write-Host "Compile a test suite DLL (MSVC):"
Write-Host "  cl /DDLL_BUILD /I`"$DestIncludeRoot`" my_tests.c /LD /OUT:my_tests.dll /link `"$DestLib\faultline.lib`""
Write-Host ""
Write-Host "CMake:"
Write-Host "  list(APPEND CMAKE_PREFIX_PATH `"$DestRoot`")"
Write-Host "  find_package(Faultline CONFIG REQUIRED)"
Write-Host "  target_link_libraries(my_tests PRIVATE Faultline::faultline)"
Write-Host ""
Write-Host "Run a test suite:"
Write-Host "  `"$DestBin\faultline.exe`" run my_tests.dll"
