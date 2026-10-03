[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$RunId,
    [ValidateRange(30, 3600)]
    [int]$Seconds = 900
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $repoRoot 'build-msvc-tailfix\RelWithDebInfo\mm3_recomp.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Missing clean build: $exe" }

$runDir = Join-Path $repoRoot "conformance_tmp\clean-strict\$RunId"
if (Test-Path -LiteralPath $runDir) { throw "Run directory already exists: $runDir" }
New-Item -ItemType Directory -Path $runDir -Force | Out-Null

$out = Join-Path $runDir 'stdout.log'
$err = Join-Path $runDir 'stderr.log'
$meta = Join-Path $runDir 'run.json'
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
$outer = (git -C $repoRoot rev-parse --short HEAD).Trim()
$toolkit = (git -C (Join-Path $repoRoot 'tools\xboxrecomp') rev-parse --short HEAD).Trim()

# Preserve save/input settings; clear diagnostic and recovery toggles.
$oldEnv = @{}
$cleared = @()
foreach ($entry in Get-ChildItem Env:) {
    if ($entry.Name -match '^(MM3_|RECOMP_|NV2A_)') {
        if ($entry.Name -in @('MM3_SAVE_DIR', 'RECOMP_KEYBOARD', 'RECOMP_FB_WINDOW')) { continue }
        $oldEnv[$entry.Name] = $entry.Value
        Remove-Item "Env:$($entry.Name)" -ErrorAction SilentlyContinue
        $cleared += $entry.Name
    }
}
$oldRunId = $env:MM3_RUN_ID
$env:MM3_RUN_ID = $RunId

$process = $null
$timedOut = $false
$exitCode = $null
try {
    $process = Start-Process -FilePath $exe -WorkingDirectory $repoRoot -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -WindowStyle Normal
    if (-not $process.WaitForExit($Seconds * 1000)) {
        $timedOut = $true
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    $process.Refresh()
    $exitCode = $process.ExitCode
} finally {
    foreach ($name in $oldEnv.Keys) {
        Set-Item "Env:$name" $oldEnv[$name]
    }
    if ($null -eq $oldRunId) {
        Remove-Item Env:MM3_RUN_ID -ErrorAction SilentlyContinue
    } else {
        $env:MM3_RUN_ID = $oldRunId
    }
}

$crash = (Test-Path -LiteralPath $err) -and (Select-String -LiteralPath $err -Pattern '\[CRASH\]|Access violation|0xC0000005' -Quiet)
$status = if ($timedOut) { 'timeout' } elseif ($crash) { 'crash' } elseif ($exitCode -ne 0) { 'failed' } else { 'process_exit_only' }
$result = [ordered]@{
    run_id = $RunId
    status = $status
    timeout_seconds = $Seconds
    timed_out = $timedOut
    exit_code = $exitCode
    executable = $exe
    sha256 = $hash
    outer_commit = $outer
    toolkit_commit = $toolkit
    keyboard_input_enabled = ($env:RECOMP_KEYBOARD -and $env:RECOMP_KEYBOARD -ne '0')
    framebuffer_window_enabled = ($env:RECOMP_FB_WINDOW -and $env:RECOMP_FB_WINDOW -ne '0')
    diagnostics_and_recovery_env_cleared = @($cleared | Sort-Object)
    save_dir_preserved = [bool]$env:MM3_SAVE_DIR
    stdout = $out
    stderr = $err
    visual_acceptance = 'not measured by this runner'
}
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $meta -Encoding utf8
Write-Host "[$RunId] status=$status exit=$exitCode exe=$exe sha256=$hash"
Write-Host "[$RunId] stdout=$out stderr=$err metadata=$meta"
if ($exitCode -ne 0 -or $timedOut -or $crash) { exit 1 }
