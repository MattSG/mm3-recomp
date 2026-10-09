param(
    [string]$SaveTemplate,
    [switch]$DevHud, [switch]$DebugMenu, [switch]$ExtendedCameras, [switch]$E3Preset,
    [switch]$CaptureDisplay,
    [ValidateSet('Retail','Default','Max')][string]$GraphicsPreset = 'Default',
    [hashtable]$Graphics = @{}
)
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
    MM3_DEV_HUD=[int]$DevHud.IsPresent; MM3_DEBUG_MENU=[int]$DebugMenu.IsPresent
    MM3_EXTENDED_CAMERAS=[int]$ExtendedCameras.IsPresent; MM3_E3_PRESET=[int]$E3Preset.IsPresent
    MM3_DEBUG_TELEPORT='1'; RECOMP_VSYNC='0'; RECOMP_MSAA='4'; RECOMP_ANISO='16'
    RECOMP_DRAW_DISTANCE='2'; MM3_FOG_REMAP='1'; RECOMP_PS_SPEC='1'
    RECOMP_WINDOW_SIZE='1280x720'; RECOMP_D3D_DEBUG=$null
}
if ($CaptureDisplay) { $settings.RECOMP_CAPTURE_SURFACE=$null }
if ($GraphicsPreset -eq 'Retail') {
    $settings.RECOMP_MSAA='1'; $settings.RECOMP_ANISO='1'
    $settings.RECOMP_DRAW_DISTANCE='1'; $settings.RECOMP_RENDER_SCALE='1'
    $settings.RECOMP_ASPECT='4:3'
} elseif ($GraphicsPreset -eq 'Max') {
    $settings.RECOMP_MSAA='8'; $settings.RECOMP_DRAW_DISTANCE='4'
    $settings.RECOMP_RENDER_SCALE='4'
}
foreach ($key in $Graphics.Keys) {
    if ($key -notin $settings.Keys -or $key -notmatch '^(RECOMP_(MSAA|ANISO|DRAW_DISTANCE|RENDER_SCALE|ASPECT|VSYNC|PS_SPEC|D3D_DEBUG)|MM3_FOG_REMAP)$') {
        throw "Unsupported graphics override: $key"
    }
    $settings[$key] = $Graphics[$key]
}
[IO.File]::WriteAllText($settings.RECOMP_PAD_LIVE, '')
$settings | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'settings.json')
$previous = @{}
try {
    foreach ($key in $settings.Keys) {
        $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        $value = if ($null -eq $settings[$key]) { [NullString]::Value } else { [string]$settings[$key] }
        [Environment]::SetEnvironmentVariable($key, $value, 'Process')
    }
    $process = Start-Process -FilePath $exe -WorkingDirectory $repo -WindowStyle Normal -PassThru `
        -RedirectStandardOutput (Join-Path $run 'stdout.log') `
        -RedirectStandardError (Join-Path $run 'stderr.log')
} finally {
    foreach ($key in $previous.Keys) {
        $value = if ($null -eq $previous[$key]) { [NullString]::Value } else { [string]$previous[$key] }
        [Environment]::SetEnvironmentVariable($key, $value, 'Process')
    }
}
@{ pid=$process.Id; executable=$exe; run=$run; save=$save } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repo 'out/debug-features/current-run.json')
"PID=$($process.Id) Run=$run"
