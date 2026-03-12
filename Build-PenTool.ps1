param(
    [string] $NukeRoot = "",
    [string] $QtRoot   = "",
    [switch] $Clean
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Header([string]$msg) { Write-Host ""; Write-Host "-- $msg" -ForegroundColor Cyan }
function Fail([string]$msg) { Write-Host "FAILED: $msg" -ForegroundColor Red; exit 1 }

function Format-BuildOutput([string[]]$lines) {
    $errs  = [System.Collections.Generic.List[string]]::new()
    $warns = [System.Collections.Generic.List[string]]::new()
    foreach ($line in $lines) {
        if ($line -match 'error\s+(C\d+|LNK\d+|MSB\d+)\s*:(.+)') {
            $f = ""; if ($line -match '([^\\]+\.\w+)\((\d+)\)') { $f = "$($matches[1]):$($matches[2]) " }
            $errs.Add("  [ERR $($matches[1].Trim())] $f$($matches[2].Trim())")
        } elseif ($line -match 'warning\s+(C\d+)\s*:(.+)') {
            $f = ""; if ($line -match '([^\\]+\.\w+)\((\d+)\)') { $f = "$($matches[1]):$($matches[2]) " }
            $warns.Add("  [WARN $($matches[1].Trim())] $f$($matches[2].Trim())")
        }
    }
    return $errs, $warns
}

Write-Header "Locating Nuke"
if (-not $NukeRoot) {
    foreach ($c in @("C:\Program Files\Nuke17.1v1","C:\Program Files\Nuke17.0v3","C:\Program Files\Nuke17.0v2","C:\Program Files\Nuke17.0v1","D:\Nuke17.0v1")) {
        if (Test-Path $c) { $NukeRoot = $c; break }
    }
}
if (-not $NukeRoot) { Fail "Nuke not found. Pass -NukeRoot" }
Write-Host "  $NukeRoot"

Write-Header "Locating Qt6"
if (-not $QtRoot) {
    foreach ($base in @("C:\Qt","D:\Qt","$env:USERPROFILE\Qt")) {
        if (-not (Test-Path $base)) { continue }
        foreach ($ver in (Get-ChildItem $base -Directory -Filter "6.*" -EA SilentlyContinue | Sort-Object Name -Descending)) {
            $c = Join-Path $ver.FullName "msvc2019_64"
            if (Test-Path "$c\lib\cmake\Qt6\Qt6Config.cmake") { $QtRoot = $c; break }
        }
        if ($QtRoot) { break }
    }
}
if (-not $QtRoot) { Fail "Qt6 not found. Pass -QtRoot" }
Write-Host "  $QtRoot"

Write-Header "Setting up MSVC x64"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Fail "vswhere.exe not found." }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null | Select-Object -First 1
if (-not $vsPath) { Fail "No Visual Studio with C++ tools found." }
foreach ($line in (& cmd.exe /c "`"$(Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat')`" x64 > nul 2>&1 && set" 2>$null)) {
    if ($line -match "^([^=]+)=(.*)$") { [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process") }
}
Write-Host "  MSVC $($env:VCToolsVersion) | $($env:VSCMD_ARG_TGT_ARCH)"

if (-not (Get-Command cmake -EA SilentlyContinue)) {
    $b = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $b) { $env:PATH = (Split-Path $b) + ";" + $env:PATH } else { Fail "cmake not found." }
}

$buildDir = Join-Path $PSScriptRoot "build"
if ($Clean -and (Test-Path $buildDir)) { Remove-Item $buildDir -Recurse -Force; Write-Host "  Cleaned" }

Write-Header "Configuring"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Push-Location $buildDir
try {
    $out = & cmake .. -G "Visual Studio 17 2022" -A x64 "-DNUKE_ROOT=$NukeRoot" "-DCMAKE_PREFIX_PATH=$QtRoot" "-DQt6_DIR=$QtRoot\lib\cmake\Qt6" 2>&1
    if ($LASTEXITCODE -ne 0) { $out | Where-Object { $_ -match "Error|FATAL" } | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }; Fail "Configure failed." }
    $out | Where-Object { $_ -match "^--" } | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }

    Write-Header "Building"
    $so = "$buildDir\_o.txt"; $se = "$buildDir\_e.txt"
    $proc = Start-Process cmake -ArgumentList "--build . --config Release --parallel" -NoNewWindow -PassThru -RedirectStandardOutput $so -RedirectStandardError $se
    $proc.WaitForExit()
    $all = @(); if (Test-Path $so) { $all += Get-Content $so }; if (Test-Path $se) { $all += Get-Content $se }
    $errs, $warns = Format-BuildOutput $all
    foreach ($w in $warns) { Write-Host $w -ForegroundColor Yellow }
    
    $dll = Join-Path $buildDir "Release\PenTool.dll"
    if (-not (Test-Path $dll)) {
        Write-Host "  Build errors:" -ForegroundColor Red
        if ($errs.Count -gt 0) { foreach ($e in $errs) { Write-Host $e -ForegroundColor Red } }
        else { $all | Select-Object -Last 30 | ForEach-Object { Write-Host "  $_" -ForegroundColor Red } }
        Fail "Build failed."
    }
    Write-Host "  Build OK ($($warns.Count) warnings)" -ForegroundColor Green

    $dll = Join-Path $buildDir "Release\PenTool.dll"
    if (-not (Test-Path $dll)) { Fail "PenTool.dll not found." }
    Write-Host "  PenTool.dll ($([math]::Round((Get-Item $dll).Length/1KB,1)) KB)" -ForegroundColor Green

    Write-Header "Installing to Nuke"
    $inst = "$env:USERPROFILE\.nuke\plugins\PenTool"
    New-Item -ItemType Directory -Force -Path $inst | Out-Null
    Copy-Item $dll "$inst\PenTool.dll" -Force
    Copy-Item "$PSScriptRoot\python\menu.py" "$inst\menu.py" -Force
    Write-Host "  PenTool.dll -> $inst" -ForegroundColor Green
    Write-Host "  menu.py     -> $inst" -ForegroundColor Green

    $ip = "$env:USERPROFILE\.nuke\init.py"
    $al = "nuke.pluginAddPath(r'$inst')"
    if (-not (Test-Path $ip)) { "import nuke`n$al" | Set-Content $ip -Encoding UTF8; Write-Host "  Created init.py" -ForegroundColor Green }
    elseif (-not (Select-String -Path $ip -Pattern ([regex]::Escape($inst)) -Quiet)) { Add-Content $ip "`n$al"; Write-Host "  Patched init.py" -ForegroundColor Green }
    else { Write-Host "  init.py already up to date" -ForegroundColor DarkGray }

} finally { Pop-Location }

Write-Host ""
Write-Host "Done -- launch Nuke and press Ctrl+Shift+P" -ForegroundColor Green
