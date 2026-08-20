<#
.SYNOPSIS
  Import, update, or remove a FaultLine service package in a consumer repository.

.DESCRIPTION
  Reads a package's manifest.txt and reconciles a consumer's vendored tree
  against it. Unlike a plain file copy, this is a true sync: files dropped from a
  package in a newer version are deleted from the consumer on update.

  All services merge into a single unified tree under -Into (the "root"):

      <root>/src/...       compile these (plus src/fnv/*.c if present)
      <root>/include/...   add <root>/include to the consumer include path
      <root>/faultline.lock   bookkeeping (which service owns which files)

  Shared files (e.g. fl_exception_service.c, which several packages bundle) land
  at the same path and are reference-counted in the lockfile: a file is deleted
  only when the last service that owns it stops shipping it. This makes it safe
  to import overlapping packages into the same tree.

.PARAMETER From
  Path to a package directory containing manifest.txt (e.g. dist\memory).

.PARAMETER Into
  Root of the consumer's vendored tree. Default: third_party/faultline.

.PARAMETER Remove
  Remove a previously-imported service by name instead of importing.

.PARAMETER List
  List the services currently recorded in the lockfile.

.PARAMETER NoDepCheck
  Skip the check that a package's declared dependencies are already installed.

.EXAMPLE
  fl_import.ps1 -From dist\memory -Into third_party\faultline

.EXAMPLE
  fl_import.ps1 -Remove memory -Into third_party\faultline
#>
[CmdletBinding(DefaultParameterSetName = 'Import')]
param(
    [Parameter(ParameterSetName = 'Import', Position = 0)][string]$From,
    [string]$Into = "$PWD\third_party\faultline",
    [Parameter(ParameterSetName = 'Remove')][string]$Remove,
    [Parameter(ParameterSetName = 'List')][switch]$List,
    [Parameter(ParameterSetName = 'Help')][Alias('h')][switch]$Help,
    [switch]$NoDepCheck
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Get-Help $PSCommandPath -Detailed
    exit 0
}

# Set-Location/cd updates PowerShell's location but NOT [Environment]::CurrentDirectory,
# the value .NET file APIs ([System.IO.File]::ReadAllBytes/WriteAllText, etc.) use to
# resolve relative paths. Left unsynced, a relative -Into would be read from / written to
# the process's startup directory instead of the caller's location. Sync it once here so
# every raw .NET call below sees the same cwd PowerShell does.
[Environment]::CurrentDirectory = $PWD.ProviderPath

# Compute a lowercase SHA-256 hex digest using .NET directly, so we do not
# depend on Get-FileHash (absent or constrained in some PowerShell hosts).
#
# .NET file APIs resolve relative paths against [Environment]::CurrentDirectory,
# which Set-Location/cd does NOT update -- so a relative $path would be read from
# the process's startup directory, not the caller's location. Convert-Path turns
# it into an absolute provider path first. We deliberately do not catch here: a
# read failure is a real error and must propagate, not be masked as a later
# "Hash mismatch (corrupt package?)".
function Get-Sha256([string]$path) {
    $full = Convert-Path -LiteralPath $path
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.IO.File]::ReadAllBytes($full)
        return ([BitConverter]::ToString($sha.ComputeHash($bytes)) -replace '-', '').ToLower()
    } finally {
        $sha.Dispose()
    }
}

function ConvertTo-LocalPath([string]$p) {
    return ($p -replace '/', [string][IO.Path]::DirectorySeparatorChar)
}

function Read-Lock([string]$lockPath) {
    $svc = [ordered]@{}
    $files = New-Object System.Collections.Generic.List[object]
    if (Test-Path -LiteralPath $lockPath) {
        foreach ($line in (Get-Content -LiteralPath $lockPath)) {
            $t = $line.Trim()
            if ($t -eq "" -or $t.StartsWith("#")) { continue }
            $p = $t -split '\s+'
            switch ($p[0]) {
                'svc' { if ($p.Count -ge 3) { $svc[$p[1]] = $p[2] } }
                'f'   { if ($p.Count -ge 3) { $files.Add([pscustomobject]@{ Svc = $p[1]; Path = $p[2] }) } }
            }
        }
    }
    return [pscustomobject]@{ Svc = $svc; Files = $files }
}

function Write-Lock([string]$lockPath, $lock) {
    $dir = Split-Path -Parent $lockPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    $out = New-Object System.Collections.Generic.List[string]
    $out.Add("# FaultLine import lockfile -- managed by fl_import. Do not edit by hand.")
    foreach ($name in $lock.Svc.Keys) {
        $out.Add("svc $name $($lock.Svc[$name])")
        # Ordinal, not Sort-Object: that cmdlet's culture-sensitive collation ranks '.'
        # against '_' differently under Windows PowerShell 5.1 (NLS) and PowerShell 7
        # (ICU), so row order would depend on which host ran the import and the
        # lockfile would churn whenever a consumer switched between them. The manifest
        # writer orders its rows the same way for the same reason.
        $paths = [string[]]@(
            $lock.Files | Where-Object { $_.Svc -eq $name } | Select-Object -ExpandProperty Path
        )
        [Array]::Sort($paths, [System.StringComparer]::Ordinal)
        foreach ($p in $paths) {
            $out.Add("f $name $p")
        }
    }
    # LF, no BOM -- the lockfile is also read by fl_import.sh, and CRLF/BOM
    # would corrupt its line parsing. (Set-Content -Encoding UTF8 emits both.)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($lockPath, ($out -join "`n") + "`n", $utf8NoBom)
}

function Read-Manifest([string]$pkgDir) {
    $mpath = Join-Path $pkgDir "manifest.txt"
    if (-not (Test-Path -LiteralPath $mpath)) {
        throw "No manifest.txt found in package: $pkgDir"
    }
    $name = $null; $version = $null; $depends = @()
    $files = New-Object System.Collections.Generic.List[object]
    foreach ($line in (Get-Content -LiteralPath $mpath)) {
        $t = $line.Trim()
        if ($t -eq "" -or $t.StartsWith("#")) { continue }
        $p = $t -split '\s+'
        switch ($p[0]) {
            'name'    { $name = $p[1] }
            'version' { $version = $p[1] }
            'depends' { if ($p.Count -ge 2) { $depends = $p[1..($p.Count - 1)] } }
            'f'       { if ($p.Count -ge 3) { $files.Add([pscustomobject]@{ Path = $p[1]; Hash = $p[2].ToLower() }) } }
        }
    }
    if (-not $name) { throw "Manifest missing 'name': $mpath" }
    if (-not $version) { throw "Manifest missing 'version': $mpath" }
    return [pscustomobject]@{ Name = $name; Version = $version; Depends = $depends; Files = $files }
}

function Remove-EmptyDirs([string]$root) {
    if (-not (Test-Path -LiteralPath $root)) { return }
    Get-ChildItem -LiteralPath $root -Recurse -Directory |
        Sort-Object { $_.FullName.Length } -Descending |
        ForEach-Object {
            if (-not (Get-ChildItem -LiteralPath $_.FullName -Force)) {
                Remove-Item -LiteralPath $_.FullName -Force
            }
        }
}

$lockPath = Join-Path $Into "faultline.lock"
$lock = Read-Lock $lockPath

# --- List ---------------------------------------------------------------
if ($List) {
    if ($lock.Svc.Count -eq 0) {
        Write-Host "No services imported under $Into"
        exit 0
    }
    Write-Host "Imported services under ${Into}:"
    foreach ($name in $lock.Svc.Keys) {
        $cnt = @($lock.Files | Where-Object { $_.Svc -eq $name }).Count
        Write-Host ("  {0,-18} {1,-10} {2} file(s)" -f $name, $lock.Svc[$name], $cnt)
    }
    exit 0
}

# --- Remove -------------------------------------------------------------
if ($Remove) {
    if (-not $lock.Svc.Contains($Remove)) {
        Write-Host "Service '$Remove' is not installed under $Into"
        exit 0
    }
    $owned = @($lock.Files | Where-Object { $_.Svc -eq $Remove } | Select-Object -ExpandProperty Path)
    $deleted = 0
    foreach ($p in $owned) {
        $others = @($lock.Files | Where-Object { $_.Svc -ne $Remove -and $_.Path -eq $p })
        if ($others.Count -eq 0) {
            $dst = Join-Path $Into (ConvertTo-LocalPath $p)
            if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Force; $deleted++ }
        }
    }
    $lock.Files = @($lock.Files | Where-Object { $_.Svc -ne $Remove })
    $lock.Svc.Remove($Remove)
    Write-Lock $lockPath $lock
    Remove-EmptyDirs $Into
    Write-Host "Removed '$Remove' ($deleted file(s) deleted; shared files kept for other services)."
    exit 0
}

# --- Import -------------------------------------------------------------
if (-not $From) {
    throw "Specify a package directory with -From, or use -List / -Remove."
}
if (-not (Test-Path -LiteralPath $From -PathType Container)) {
    throw "Package directory not found: $From"
}
$pkg = Read-Manifest $From

if (-not $NoDepCheck -and @($pkg.Depends).Count -gt 0) {
    $missing = @()
    foreach ($d in $pkg.Depends) {
        if ($d -and -not $lock.Svc.Contains($d)) { $missing += $d }
    }
    if ($missing.Count -gt 0) {
        throw "Service '$($pkg.Name)' depends on: $($missing -join ', '). Import those first, or re-run with -NoDepCheck."
    }
}

$added = 0; $updated = 0; $conflicts = 0
foreach ($f in $pkg.Files) {
    $src = Join-Path $From (ConvertTo-LocalPath $f.Path)
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Manifest lists '$($f.Path)' but it is missing from the package."
    }
    $dst = Join-Path $Into (ConvertTo-LocalPath $f.Path)
    $dstDir = Split-Path -Parent $dst
    if (-not (Test-Path -LiteralPath $dstDir)) {
        New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    }

    if (Test-Path -LiteralPath $dst) {
        $curHash = Get-Sha256 $dst
        $otherOwners = @($lock.Files | Where-Object { $_.Path -eq $f.Path -and $_.Svc -ne $pkg.Name })
        if ($otherOwners.Count -gt 0 -and $curHash -ne $f.Hash) {
            $owners = (@($otherOwners | Select-Object -ExpandProperty Svc) | Sort-Object -Unique) -join ', '
            Write-Warning "Shared file '$($f.Path)' differs from the copy owned by [$owners]; overwriting with '$($pkg.Name)' version."
            $conflicts++
        }
    }

    Copy-Item -LiteralPath $src -Destination $dst -Force
    $newHash = Get-Sha256 $dst
    if ($newHash -ne $f.Hash) {
        throw "Hash mismatch after copying '$($f.Path)' (corrupt package?)."
    }

    $ownedBySvc = @($lock.Files | Where-Object { $_.Path -eq $f.Path -and $_.Svc -eq $pkg.Name })
    if ($ownedBySvc.Count -gt 0) { $updated++ } else { $added++ }
}

# Stale removal: files this service used to ship but no longer does.
$newPaths = @($pkg.Files | Select-Object -ExpandProperty Path)
$oldPaths = @($lock.Files | Where-Object { $_.Svc -eq $pkg.Name } | Select-Object -ExpandProperty Path)
$removed = 0
foreach ($p in $oldPaths) {
    if ($newPaths -contains $p) { continue }
    $others = @($lock.Files | Where-Object { $_.Svc -ne $pkg.Name -and $_.Path -eq $p })
    if ($others.Count -eq 0) {
        $dst = Join-Path $Into (ConvertTo-LocalPath $p)
        if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Force; $removed++ }
    }
}

# Rewrite this service's ownership in the lockfile.
$lock.Files = @($lock.Files | Where-Object { $_.Svc -ne $pkg.Name })
foreach ($p in $newPaths) {
    $lock.Files += [pscustomobject]@{ Svc = $pkg.Name; Path = $p }
}
$lock.Svc[$pkg.Name] = $pkg.Version
Write-Lock $lockPath $lock
Remove-EmptyDirs $Into

Write-Host ""
Write-Host "Imported '$($pkg.Name)' v$($pkg.Version) into $Into"
Write-Host ("  {0} added, {1} updated, {2} removed (stale), {3} shared-file conflict(s)" -f $added, $updated, $removed, $conflicts)
