param(
    [Parameter(Mandatory)][string]$Python,
    [Parameter(Mandatory)][string]$GeneratorDir,
    [Parameter(Mandatory)][string]$ManualFunctions
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$Python = (Resolve-Path $Python).Path
$ManualFunctions = (Resolve-Path $ManualFunctions).Path
$GeneratorDir = [IO.Path]::GetFullPath($GeneratorDir)
# Native F7F58 saves EBX/ESI/EDI and restores them at F80AC before RET 8.
# The heuristic F7FCF/F8000 entries split that method and leak ESI=0x2000
# into the GUI child factory. Keep the native branches and epilogue together.
Push-Location (Join-Path $repo 'tools/xboxrecomp')
try {
    $functions = Join-Path $GeneratorDir 'gui_boundary_functions.json'
    & $Python (Join-Path $repo 'tools/prepare_mm3_gui_boundaries.py') --output $functions
    if ($LASTEXITCODE -ne 0) { throw 'Boundary evidence preparation failed' }
    & $Python -m tools.recomp (Join-Path $repo 'game_files/default.xbe') --all --split 1000 --gen-dir $GeneratorDir --functions $functions --manual-functions $ManualFunctions --coalesce-functions (Join-Path $repo 'mm3_function_coalescences.json') --skip-binary-check
    if ($LASTEXITCODE -ne 0) { throw "Generation failed: $LASTEXITCODE" }
} finally { Pop-Location }
