<#
.SYNOPSIS
  Collect FaultLine service distribution packages into dist\.

.DESCRIPTION
  Copies the files that make up a service package out of the repository and into
  dist\<name>\, then calls fl_emit_manifest.ps1 to record what was collected.

  What each package contains is data, held in packages.psd1; this script is the
  logic that acts on it. Adding a file to a package is an edit to that data file,
  not to any script.

  Sources are copied verbatim, so a file in a package is byte-identical to its
  original in the repository. Four roots feed a package:

      Src -> src\                  becomes dist\<name>\src\
      Inc -> include\faultline\    becomes dist\<name>\include\faultline\
      Top -> include\              becomes dist\<name>\include\
      Fnv -> third_party\fnv\      becomes dist\<name>\src\fnv\

  Every listed file is resolved before anything is removed, so a package that
  names a file that does not exist fails without leaving a half-collected tree
  behind. Collection wipes src\ and include\ but keeps manifest.txt, which
  fl_emit_manifest.ps1 reads to decide whether anything actually changed.

.PARAMETER Package
  Name of a single package to collect, as keyed in packages.psd1.

.PARAMETER All
  Collect every package in the spec.

.PARAMETER Clean
  Remove the selected package directories instead of collecting them.

.PARAMETER List
  Print the packages the spec defines, with version and dependencies.

.PARAMETER SpecPath
  Override the spec file. Defaults to packages.psd1 beside this script.

.EXAMPLE
  fl_dist.ps1 -Package log_service

.EXAMPLE
  fl_dist.ps1 -All

.EXAMPLE
  fl_dist.ps1 -Package memory_service -Clean
#>
param(
    [Alias('h')]
    [switch]$Help,

    [string]$Package,
    [switch]$All,
    [switch]$Clean,
    [switch]$List,
    [string]$SpecPath
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Get-Help $PSCommandPath -Detailed
    exit 0
}

# build\dist\fl_dist.ps1 -> the repository root two levels up.
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path.TrimEnd('\', '/')

if (-not $SpecPath) {
    $SpecPath = Join-Path $PSScriptRoot "packages.psd1"
}
if (-not (Test-Path -LiteralPath $SpecPath -PathType Leaf)) {
    Write-Host "ERROR: package spec not found: $SpecPath"
    exit 1
}

$SpecPath = (Resolve-Path -LiteralPath $SpecPath).Path

# Read the spec through the parser rather than Import-PowerShellDataFile. That
# cmdlet is unavailable to Windows PowerShell whenever $Env:PSModulePath has been
# rewritten by a PowerShell 7 parent -- which is what running this from a pwsh
# prompt by way of cmd does, leaving the v1.0 module directory off the path so
# Microsoft.PowerShell.Utility never autoloads. SafeGetValue needs no module, and
# keeps the spec data-only: it evaluates literals and refuses anything executable.
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($SpecPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors) {
    Write-Host "ERROR: $SpecPath could not be parsed:"
    foreach ($parseError in $parseErrors) {
        Write-Host "  line $($parseError.Extent.StartLineNumber): $($parseError.Message)"
    }
    exit 1
}
$hashAst = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.HashtableAst] }, $false)
if (-not $hashAst) {
    Write-Host "ERROR: $SpecPath does not contain a hashtable."
    exit 1
}

$spec = $hashAst.SafeGetValue()
$packages = $spec.Packages
$known = @($packages.Keys | Sort-Object)

if ($List) {
    foreach ($n in $known) {
        $p = $packages[$n]
        $dep = $p.Depends
        if (-not $dep) { $dep = "-" }
        Write-Host ("{0,-22} {1,-8} depends: {2}" -f $n, $p.Version, $dep)
    }
    exit 0
}

if ($All -and $Package) {
    Write-Host "ERROR: -All and -Package are mutually exclusive."
    exit 1
}
if ($All) {
    $selected = $known
} elseif ($Package) {
    if (-not $packages.ContainsKey($Package)) {
        Write-Host "ERROR: unknown package '$Package'. Known packages: $($known -join ', ')"
        exit 1
    }
    $selected = @($Package)
} else {
    Write-Host "ERROR: specify -Package <name>, -All, or -List."
    exit 1
}

# Where each bucket is read from, and where under the package it lands.
$roots = @{
    Src = @{ From = (Join-Path $repo "src");                To = "src" }
    Inc = @{ From = (Join-Path $repo "include\faultline");  To = "include\faultline" }
    Top = @{ From = (Join-Path $repo "include");            To = "include" }
    Fnv = @{ From = (Join-Path $repo "third_party\fnv");    To = "src\fnv" }
}
$buckets = @("Src", "Inc", "Top", "Fnv")

$failed = 0

foreach ($name in $selected) {
    $p = $packages[$name]
    $dist = Join-Path $repo (Join-Path "dist" $name)

    if ($Clean) {
        if (Test-Path -LiteralPath $dist) {
            Write-Host "Removing $dist"
            Remove-Item -LiteralPath $dist -Recurse -Force
        } else {
            Write-Host "Nothing to clean for $name."
        }
        continue
    }

    Write-Host ""
    Write-Host "$name -- $($p.Title)"

    # Resolve everything the package names before removing anything, so a bad
    # spec cannot destroy a good package.
    $plan = New-Object System.Collections.Generic.List[object]
    $missing = New-Object System.Collections.Generic.List[string]
    foreach ($bucket in $buckets) {
        if (-not $p.ContainsKey($bucket)) { continue }
        foreach ($file in $p[$bucket]) {
            $from = Join-Path $roots[$bucket].From $file
            if (-not (Test-Path -LiteralPath $from -PathType Leaf)) {
                $missing.Add("$bucket $file  (looked in $($roots[$bucket].From))")
                continue
            }
            $plan.Add([pscustomobject]@{
                From = $from
                To   = (Join-Path $dist (Join-Path $roots[$bucket].To $file))
            })
        }
    }

    if ($missing.Count -gt 0) {
        Write-Host "  ERROR: the spec names files that do not exist:"
        foreach ($m in $missing) {
            Write-Host "    $m"
        }
        $failed++
        continue
    }

    # Wipe the collected trees so the package reflects exactly the current file
    # set. manifest.txt is deliberately left in place: fl_emit_manifest.ps1 reads
    # it to tell an unchanged package from a changed one.
    foreach ($sub in @("src", "include")) {
        $subPath = Join-Path $dist $sub
        if (Test-Path -LiteralPath $subPath) {
            Remove-Item -LiteralPath $subPath -Recurse -Force
        }
    }

    foreach ($item in $plan) {
        $destDir = Split-Path -Parent $item.To
        if (-not (Test-Path -LiteralPath $destDir)) {
            New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        }
        Copy-Item -LiteralPath $item.From -Destination $item.To -Force
    }
    Write-Host "  copied $($plan.Count) files"

    & (Join-Path $PSScriptRoot "fl_emit_manifest.ps1") `
        -PackageDir $dist -Name $name -Version $p.Version -Depends $p.Depends
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ERROR: manifest generation failed."
        $failed++
        continue
    }

    # A package that ships private headers under src\ needs that directory on the
    # consumer's include path too.
    $privateHeaders = $false
    if ($p.ContainsKey("Src")) {
        foreach ($f in $p.Src) {
            if ($f.EndsWith(".h")) { $privateHeaders = $true }
        }
    }
    $hasSources = $false
    foreach ($bucket in @("Src", "Fnv")) {
        if ($p.ContainsKey($bucket)) {
            foreach ($f in $p[$bucket]) {
                if ($f.EndsWith(".c")) { $hasSources = $true }
            }
        }
    }

    if ($p.Depends) {
        Write-Host "  Depends on            : $($p.Depends) (import it first)"
    }
    if ($privateHeaders) {
        Write-Host "  Consumer include paths:"
        Write-Host "    dist\$name\include   (public headers)"
        Write-Host "    dist\$name\src       (private headers)"
        Write-Host "  Compile these sources:"
        Write-Host "    dist\$name\src\*.c"
    } else {
        Write-Host "  Consumer include path : dist\$name\include"
        if ($hasSources) {
            Write-Host "  Compile these sources : dist\$name\src\*.c"
        } else {
            Write-Host "  Compile these sources : none (header-only)"
        }
    }
}

if ($failed -gt 0) {
    Write-Host ""
    Write-Host "ERROR: $failed package(s) could not be collected."
    exit 1
}

exit 0
