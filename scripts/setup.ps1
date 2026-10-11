<#
.SYNOPSIS
Build a playable Midtown Madness 3 from a fresh clone and your own disc image.

.DESCRIPTION
  pwsh scripts\setup.ps1 -Iso "D:\Midtown Madness 3 (USA).iso"

1. checks for Visual Studio or local MSVC tools, and Python 3.11.4+
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
. (Join-Path $PSScriptRoot 'enter-env.ps1')
function Step([string]$text) { Write-Host "`n== $text" -ForegroundColor Cyan }
function Check([string]$what) { if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" } }

Step 'Prerequisites'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = if (Test-Path $vswhere) {
    & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
$portableMsvc = Test-Path -LiteralPath (Join-Path $repo 'tools\bin\msvc\setup_x64.bat')
if (-not $vs -and -not $portableMsvc) {
    throw 'Visual Studio 2022+ with C++ tools, or local tools\bin\msvc\setup_x64.bat, is required.'
}
$cmake = if ($vs -and -not $portableMsvc) {
    Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (-not $cmake -or -not (Test-Path -LiteralPath $cmake)) {
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
}
if (-not $cmake) { throw 'CMake not found: install CMake locally or the VS CMake component.' }
if ($portableMsvc -and -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw 'Ninja not found: install it in tools\venv with python -m pip install ninja.'
}
$py = Get-Command py -ErrorAction SilentlyContinue
$basePython = if ($py) { $py.Source } else { (Get-Command python -ErrorAction SilentlyContinue).Source }
if (-not $basePython) { throw 'Python 3.11.4+ is required (py or python on PATH; 3.12+ recommended).' }
[string[]]$pythonArgs = if ($py) { @('-3') } else { @() }
& $basePython @pythonArgs -c 'import sys; sys.exit(sys.version_info < (3, 11, 4))'
if ($LASTEXITCODE -ne 0) { throw 'Python 3.11.4 or newer is required (3.12+ recommended).' }
if (-not (Test-Path 'tools\xboxrecomp\CMakeLists.txt')) {
    git submodule update --init --recursive; Check 'git submodule update'
}
& $basePython @pythonArgs scripts\apply_runtime_patch.py; Check 'MM3 runtime fixes'
if ($portableMsvc) { Write-Host "Local MSVC: $(Join-Path $repo 'tools\bin\msvc')" }
else { Write-Host "Visual Studio: $vs" }

Step 'Python environment (tools\venv)'
$python = Join-Path $repo 'tools\venv\Scripts\python.exe'
if ($Force -or -not (Test-Path $python)) {
    # Resolve outside an active venv so -Force does not overwrite a running Python.
    $pythonHome = & $basePython @pythonArgs -c 'import sys; print(sys.base_prefix)'
    Check 'Python location'
    & (Join-Path $pythonHome 'python.exe') -m venv tools\venv; Check 'venv'
}
. (Join-Path $PSScriptRoot 'enter-env.ps1')
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
if ($portableMsvc) {
    & $cmake -S . -B build -G 'Ninja Multi-Config'; Check 'cmake configure'
} else {
    & $cmake -S . -B build -A x64; Check 'cmake configure'
}
& $cmake --build build --config $Config --target mm3_recomp --parallel; Check 'build'

$exe = Join-Path $repo "build\$Config\mm3_recomp.exe"
Write-Host "`nBuilt $exe" -ForegroundColor Green
Write-Host 'Play:  pwsh scripts\play.ps1    (-Keyboard to drive with the keyboard)'
