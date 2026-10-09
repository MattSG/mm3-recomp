param([string]$SaveTemplate)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$exe = Join-Path $repo 'build/Release/mm3_recomp.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Build the Release target first.' }
if (Get-CimInstance Win32_Process -Filter "Name = 'mm3_recomp.exe'" |
    Where-Object { $_.ExecutablePath -eq $exe }) { throw 'This experiment is already running.' }
$save = Join-Path $repo 'out/debug-features/save'
if ($SaveTemplate) {
    if (Test-Path -LiteralPath $save) { throw 'Private save already exists; omit SaveTemplate.' }
    Copy-Item -LiteralPath $SaveTemplate -Destination $save -Recurse
}
New-Item -ItemType Directory -Force -Path $save | Out-Null
$run = Join-Path $repo ('out/debug-features/run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $run | Out-Null
$settings = @{
    RECOMP_PB_EXEC='1'; RECOMP_FB_WINDOW='1'; RECOMP_USB='1'
    RECOMP_APU_DSP_ACK='0x80458810'; RECOMP_FPS_LIMIT='60'
    RECOMP_ASPECT='16:9'; RECOMP_RENDER_SCALE='2'; MM3_LOG_UNBUFFERED='1'
    RECOMP_PAD_LIVE=(Join-Path $run 'pad.txt'); RECOMP_PAD_SCRIPT=$null
    RECOMP_PAD_PRESS=$null; MM3_SAVE_DIR=$save
    MM3_DEBUG_FEATURE_REQUEST=(Join-Path $run 'request.txt')
    RECOMP_PRESENT_CAPTURE=(Join-Path $run 'capture'); RECOMP_CAPTURE_SURFACE='1'
}
[IO.File]::WriteAllText($settings.RECOMP_PAD_LIVE, '')
$previous = @{}
try {
    foreach ($key in $settings.Keys) {
        $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $settings[$key], 'Process')
    }
    $process = Start-Process -FilePath $exe -WorkingDirectory $repo -WindowStyle Normal -PassThru `
        -RedirectStandardOutput (Join-Path $run 'stdout.log') `
        -RedirectStandardError (Join-Path $run 'stderr.log')
} finally {
    foreach ($key in $previous.Keys) {
        [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process')
    }
}
@{ pid=$process.Id; executable=$exe; run=$run; save=$save } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repo 'out/debug-features/current-run.json')
"PID=$($process.Id) Run=$run"
