param(
    [string]$GeneratorDir = 'conformance_tmp/mm3_seeded_gen_20261004',
    [string]$Python = 'tools/xboxrecomp/conformance_tmp/mm3_disasm_venv/Scripts/python.exe'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$pythonPath = Join-Path $repo $Python
$outDir = Join-Path $repo $GeneratorDir
$entries = Get-Content (Join-Path $repo 'mm3_runtime_entry_repairs.json') -Raw | ConvertFrom-Json
$functions = @(Get-Content (Join-Path $repo 'tools/xboxrecomp/tools/disasm/output/functions.json') -Raw | ConvertFrom-Json)
foreach ($entry in $entries) {
    $start = [Convert]::ToUInt32($entry.start.Substring(2), 16)
    $end = [Convert]::ToUInt32($entry.end.Substring(2), 16)
    $functions = @($functions | Where-Object { $_.start -ne $entry.start })
    $functions += [pscustomobject]@{
        start = $entry.start; end = $entry.end; size = $end - $start
        name = ('sub_{0:X8}' -f $start); section = '.text'; confidence = 1.0
        detection_method = 'runtime_indirect_call'; has_prologue = ($start -eq 0x104DD3)
        calls_to = @(); called_by = @(); num_instructions = 200
    }
}
$functionsPath = Join-Path $outDir 'runtime_entry_functions.json'
$functions | ConvertTo-Json -Depth 12 | Set-Content $functionsPath -Encoding utf8
& $pythonPath (Join-Path $PSScriptRoot 'Generate-MM3RuntimeEntries.py') $outDir
if ($LASTEXITCODE -ne 0) { throw 'Runtime entry generation failed' }
