<#
.SYNOPSIS
Open the local MM3 visual car/stat editor. No packages need to be installed.
#>
param(
    [string]$Config = (Join-Path $PSScriptRoot 'cars.json'),
    [string]$GameDir = (Join-Path $PSScriptRoot '..\..\game_files')
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$python = Join-Path $repo 'tools\venv\Scripts\pythonw.exe'
if (-not (Test-Path -LiteralPath $python)) {
    $python = Join-Path $repo 'tools\venv\Scripts\python.exe'
}
if (-not (Test-Path -LiteralPath $python)) {
    throw 'Python environment missing. Run scripts\setup.ps1 first, or launch editor.py with Python 3 and Tkinter.'
}
& $python (Join-Path $PSScriptRoot 'editor.py') --config $Config --game-dir $GameDir
if ($LASTEXITCODE -ne 0) { throw "Car editor exited with code $LASTEXITCODE." }
