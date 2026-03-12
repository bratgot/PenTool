param(
    [string] $NukeRoot = "",
    [switch] $InstallPlugin,
    [switch] $Clean
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Header([string]$msg) { Write-Host ""; Write-Host "── $msg" -ForegroundColor Cyan }
function Fail([string]$msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }

Write-Header "Locating Nuke 17"
if (-not $NukeRoot) {
    foreach ($c in @("C:\Program Files\Nuke17.1v1","C:\Program Files\Nuke17.0v3","C:\Program Files\Nuke17.0v2","C:\Program Files\Nuke17.0v1","C:\Program Files\Nuke16.0v5","D:\Nuke17.0v1")) {
        if (Test-Path $c) { $NukeRoot = $c; break }
    }
}
if (-not $NukeRoot -or -not (Test-Path $NukeRoot)) { Fail "Could not find Nuke. Pass -NukeRoot `"C:\Program Files\Nuke17.0v1`"" }
Write-Host "  Nuke root : $NukeRoot"
if (-not (Test-Path (Join-Path $NukeRoot "include\DDImage\Op.h"))) { Fail "NDK headers not found inside $NukeRoot" }

Write-Header "Locating Visual Studio"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Fail "vswhere.exe not found. Install Visual Studio 2019 or 2022 with the Desktop C++ workload." }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null | Select-Object -First 1
if (-not $vsPath) { Fail "No Visual Studio with C++ tools found." }
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { Fail "vcvars64.bat not found at $vcvars" }
Write-Host "  VS path   : $vsPath"

Write-Header "Importing x64 MSVC environment"
foreach ($line in (& cmd.exe /c "`"$vcvars`" x64 > nul 2>&1 && set" 2>$null)) {
    if ($line -match "^([^=]+)=(.*)$") { [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process") }
}
Write-Host "  Toolset   : $($env:VCToolsVersion)"
if ($env:VSCMD_ARG_TGT_ARCH -ne "x64") { Fail "MSVC environment is not x64." }

Write-Header "Locating CMake"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $bundled = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $bundled) { $env:PATH = (Split-Path $bundled) + ";" + $env:PATH }
    else { Fail "cmake not found. Install CMake or include it via Visual Studio." }
}
Write-Host "  $(& cmake --version | Select-Object -First 1)"

$buildDir = Join-Path $PSScriptRoot "build"
if ($Clean -and (Test-Path $buildDir)) {
    Write-Header "Cleaning"; Remove-Item $buildDir -Recurse -Force; Write-Host "  Removed $buildDir"
}

Write-Header "Configuring"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Push-Location $buildDir
try {
    & cmake .. -G "Visual Studio 17 2022" -A x64 "-DNUKE_ROOT=$NukeRoot"
    if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed." }

    Write-Header "Building Release"
    & cmake --build . --config Release --parallel
    if ($LASTEXITCODE -ne 0) { Fail "Build failed." }

    $dll = Join-Path $buildDir "Release\PenTool.dll"
    if (-not (Test-Path $dll)) { Fail "PenTool.dll not found at $dll" }
    Write-Host "  Built: $dll ($([math]::Round((Get-Item $dll).Length/1KB,1)) KB)" -ForegroundColor Green

    Write-Header "Verifying exports"
    if (Get-Command dumpbin -ErrorAction SilentlyContinue) {
        $exports = & dumpbin /exports $dll 2>$null
        if ($exports | Select-String "MMSInit")   { Write-Host "  MMSInit   : OK" -ForegroundColor Green } else { Write-Host "  MMSInit   : MISSING" -ForegroundColor Red }
        if ($exports | Select-String "MMSLoaded") { Write-Host "  MMSLoaded : OK" -ForegroundColor Green } else { Write-Host "  MMSLoaded : MISSING" -ForegroundColor Red }
    } else { Write-Host "  dumpbin not in PATH — skipping" }

    if ($InstallPlugin) {
        Write-Header "Installing"
        $installDir = Join-Path $env:USERPROFILE ".nuke\plugins\PenTool"
        New-Item -ItemType Directory -Force -Path $installDir | Out-Null
        Copy-Item $dll $installDir -Force
        Copy-Item "$PSScriptRoot\python\menu.py" $installDir -Force
        Write-Host "  Installed to: $installDir" -ForegroundColor Green
        $initPy = Join-Path $env:USERPROFILE ".nuke\init.py"
        $line = "nuke.pluginAddPath(r'$installDir')"
        if (-not (Test-Path $initPy)) { "import nuke`n$line" | Set-Content $initPy; Write-Host "  Created init.py" }
        elseif (-not (Select-String -Path $initPy -Pattern ([regex]::Escape($installDir)) -Quiet)) { Add-Content $initPy "`n$line"; Write-Host "  Patched init.py" }
        else { Write-Host "  init.py already up to date" }
    }
} finally { Pop-Location }

Write-Host ""
Write-Host "Done." -ForegroundColor Green
if (-not $InstallPlugin) { Write-Host "Tip: run with -InstallPlugin to deploy automatically." }
