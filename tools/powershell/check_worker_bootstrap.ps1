param([string]$SourceFile = 'src/recomp_manual.c')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$checkDir = Join-Path $repo 'conformance_tmp\worker_bootstrap_check'
New-Item -ItemType Directory -Path $checkDir -Force | Out-Null
# Compile these exact source bodies, without unrelated title functions.
$source = Get-Content -LiteralPath $SourceFile -Raw
$bodies = foreach ($name in @('sub_00094FC0', 'sub_00083A6C', 'sub_001E7B8F')) {
    $body = [regex]::Match($source, "(?ms)^void $name\(void\)\s*\{.*?^\}")
    if (-not $body.Success) { throw "Missing source body $name" }
    $body.Value
}
$bodies -join "`n" | Set-Content (Join-Path $checkDir 'worker_bootstrap_body.inc') -Encoding ascii
Copy-Item -LiteralPath "$repo\src\mm3_recomp_types.h" -Destination (Join-Path $checkDir 'recomp_types.h') -Force
$exe = Join-Path $checkDir 'check_worker_bootstrap.exe'
& cl.exe /nologo /W3 /I "$repo\tools\xboxrecomp\include" /I $checkDir "$PSScriptRoot\check_worker_bootstrap.c" "/Fo:$checkDir\check_worker_bootstrap.obj" "/Fe:$exe"
if ($LASTEXITCODE) { throw 'Bootstrap check compilation failed' }
& $exe
if ($LASTEXITCODE) { throw 'Bootstrap check failed' }
