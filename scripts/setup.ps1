<#
.SYNOPSIS
Build a playable Midtown Madness 3 from a fresh clone and your own disc image.

.DESCRIPTION
  pwsh scripts\setup.ps1 -Iso "D:\Midtown Madness 3 (USA).iso"

1. checks for Visual Studio (C++ workload) and Python 3.11+
2. makes tools\venv with the pinned generator dependency (capstone)
3. verifies the disc's code and extracts it to game_files\
4. generates the translated C into src\recomp\gen\ (about 6 minutes)
5. builds build\<Config>\mm3_recomp.exe

Then run scripts\play.ps1. Re-running skips finished steps; -Force redoes them.
#>
param(
    [string]$Iso,
    [ValidateSet('RelWithDebInfo', 'Release')][string]$Config = 'RelWithDebInfo',
    [switch]$SkipIsoHash,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repo
function Step([string]$text) { Write-Host "`n== $text" -ForegroundColor Cyan }
function Check([string]$what) { if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" } }

Step 'Prerequisites'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = if (Test-Path $vswhere) {
    & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $vs) { throw 'Visual Studio 2022 or newer with "Desktop development with C++" is required.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source }
if (-not $cmake) { throw 'CMake not found: install the "C++ CMake tools for Windows" VS component.' }
$py = Get-Command py -ErrorAction SilentlyContinue
if (-not $py) { throw 'Python 3.11+ is required (the py launcher from python.org).' }
& py -3 -c 'import sys; sys.exit(sys.version_info < (3, 11))'
if ($LASTEXITCODE -ne 0) { throw 'Python 3.11 or newer is required.' }
if (-not (Test-Path 'tools\xboxrecomp\CMakeLists.txt')) {
    git submodule update --init --recursive; Check 'git submodule update'
}
Write-Host "Visual Studio: $vs"

Step 'Python environment (tools\venv)'
$python = Join-Path $repo 'tools\venv\Scripts\python.exe'
if ($Force -or -not (Test-Path $python)) {
    & py -3 -m venv tools\venv; Check 'venv'
}
# The generator's output depends on the disassembler, so the version is pinned.
& $python -m pip install --quiet --disable-pip-version-check capstone==5.0.9; Check 'pip install'

Step 'Game files (game_files\)'
if ($Force -or -not (Test-Path 'game_files\default.xbe')) {
    if (-not $Iso) { throw 'Pass -Iso <your Midtown Madness 3 disc image>.' }
    $extract = @('scripts\extract_disc.py', $Iso)
    if ($SkipIsoHash) { $extract += '--skip-iso-hash' }
    & $python @extract; Check 'disc extraction'
} else {
    Write-Host 'game_files\default.xbe present (use -Force to re-extract)'
}

Step 'Generate translated code (src\recomp\gen\)'
if ($Force -or -not (Test-Path 'src\recomp\gen\recomp_dispatch.c')) {
    & $python scripts\generate_mm3.py; Check 'generation'
} else {
    Write-Host 'src\recomp\gen present (use -Force to regenerate)'
}

Step "Build ($Config)"
& $cmake -S . -B build -A x64; Check 'cmake configure'
& $cmake --build build --config $Config --target mm3_recomp --parallel; Check 'build'

$exe = Join-Path $repo "build\$Config\mm3_recomp.exe"
Write-Host "`nBuilt $exe" -ForegroundColor Green
Write-Host 'Play:  pwsh scripts\play.ps1    (-Keyboard to drive with the keyboard)'
