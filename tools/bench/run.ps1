<#
Repeatable menu and race benchmark with an isolated save copy.
  pwsh tools\bench\run.ps1 -Save <save template> [-Profile Balanced] [-Tag name]
Retains settings, executable identity, frame CSV and results under
conformance_tmp\bench\<tag>. Unreached scenes and game exits fail the run.
#>
[CmdletBinding()]
param(
    [string]$Exe,
    [Parameter(Mandatory)][string]$Save,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$')]
    [string]$Tag = ('bench-' + [guid]::NewGuid().ToString('N')),
    [ValidateSet('Balanced', 'Quality', 'LowPower')][string]$Profile = 'Balanced',
    [ValidateSet('Stationary', 'Driving')][string]$Scenario = 'Stationary',
    [switch]$HitchTrace,
    [ValidateRange(1, 90)][int]$FpsLimit,
    [ValidateRange(5, 300)][int]$Secs = 20,
    [ValidateRange(1, 255)][int]$ParticleBudget = 24,
    [string]$ShaderCacheSeed,
    [double]$X, [double]$Z,
    [double]$Heading = 0
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
if (-not $Exe) { $Exe = Join-Path $repo 'build\RelWithDebInfo\mm3_recomp.exe' }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$Save = (Resolve-Path -LiteralPath $Save).Path
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw 'Exe must be a file.' }
if (-not (Test-Path -LiteralPath $Save -PathType Container)) { throw 'Save must be a directory.' }
if ([double]::IsNaN($X) -or [double]::IsInfinity($X) -or [double]::IsNaN($Z) -or [double]::IsInfinity($Z)) {
    throw 'Coordinates must be finite.'
}
if ([double]::IsNaN($Heading) -or [double]::IsInfinity($Heading)) {
    throw 'Heading must be finite.'
}
if ($PSBoundParameters.ContainsKey('X') -ne $PSBoundParameters.ContainsKey('Z')) {
    throw 'Supply both X and Z, or omit both to measure the saved scene spawn.'
}
$explicitPosition = $PSBoundParameters.ContainsKey('X')
$benchRoot = [IO.Path]::GetFullPath((Join-Path $repo 'conformance_tmp\bench'))
$dir = [IO.Path]::GetFullPath((Join-Path $benchRoot $Tag))
if (-not $dir.StartsWith($benchRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Run directory must be inside conformance_tmp\bench.'
}
if ($dir.StartsWith($Save.TrimEnd('\') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The benchmark directory cannot be inside its save template.'
}
if ($ShaderCacheSeed) {
    $ShaderCacheSeed = (Resolve-Path -LiteralPath $ShaderCacheSeed).Path
    if (-not (Test-Path -LiteralPath $ShaderCacheSeed -PathType Container)) { throw 'ShaderCacheSeed must be a directory.' }
}
if (Test-Path -LiteralPath $dir) { throw "Run directory already exists: $dir" }
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$runSave = Join-Path $dir 'save'
Copy-Item -LiteralPath $Save -Destination $runSave -Recurse
$cachedArchive = Join-Path $Save 'Cache\Data\Data_hd.zip'
if (Test-Path -LiteralPath $cachedArchive -PathType Leaf) {
    (Get-Item -LiteralPath (Join-Path $runSave 'Cache\Data\Data_hd.zip')).CreationTimeUtc =
        (Get-Item -LiteralPath $cachedArchive).CreationTimeUtc
}
$shaderCache = Join-Path $dir 'shader-cache'
New-Item -ItemType Directory -Path $shaderCache | Out-Null
if ($ShaderCacheSeed) {
    Get-ChildItem -LiteralPath $ShaderCacheSeed -Force | Copy-Item -Destination $shaderCache -Recurse
}
$pad = Join-Path $dir 'pad.txt'
[IO.File]::WriteAllText($pad, '')
$csv = Join-Path $dir 'frames.csv'
$log = Join-Path $dir 'stderr.log'
$settings = switch ($Profile) {
    'Quality' { @{ scale='3.0'; msaa='8'; distance='2'; fps=60 } }
    'LowPower' { @{ scale='1.5'; msaa='1'; distance='1'; fps=30 } }
    default { @{ scale='2.0'; msaa='4'; distance='2'; fps=60 } }
}
if (-not $PSBoundParameters.ContainsKey('FpsLimit')) { $FpsLimit = $settings.fps }
$envs = @{
    RECOMP_PB_EXEC='1'; RECOMP_FB_WINDOW='1'; RECOMP_USB='1'; RECOMP_PAD_LIVE=$pad
    RECOMP_APU_DSP_ACK='0x80458810'; MM3_SAVE_DIR=$runSave; MM3_PERFORMANCE_PROFILE=$Profile
    RECOMP_ASPECT='16:10'; RECOMP_WINDOW_SIZE='1280x800'; RECOMP_RENDER_SCALE=$settings.scale
    RECOMP_MSAA=$settings.msaa; RECOMP_DRAW_DISTANCE=$settings.distance; RECOMP_ANISO='16'
    RECOMP_FPS_LIMIT="$FpsLimit"; RECOMP_PAUSE_UNFOCUSED='0'; RECOMP_D3D_PROFILE='1'; RECOMP_FRAME_CSV=$csv
    MM3_DEBUG_TELEPORT='1'; MM3_PARTICLE_MAX_OVERDRAW="$ParticleBudget"; RECOMP_SHADER_CACHE='1'
    RECOMP_SHADER_CACHE_DIR=$shaderCache; RECOMP_PAD_PRESS=$null; RECOMP_PAD_SCRIPT=$null
    RECOMP_HITCH_TRACE=$(if ($HitchTrace) { '1' } else { $null }); RECOMP_PRESENT_CAPTURE=$null; RECOMP_CAPTURE_SURFACE=$null
}
$previous = @{}
$p = $null
$gpuProcess = $null
$windows = @()
$teleport = Join-Path $repo 'tools\powershell\Send-MM3Teleport.ps1'
function Assert-GameRunning {
    if ($p.HasExited) { throw "Game exited with code $($p.ExitCode); see $log" }
}
function Wait-Game([double]$seconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $seconds) { Assert-GameRunning; Start-Sleep -Milliseconds 250 }
    Assert-GameRunning
}
function Press-Pad([string]$button, [double]$after) {
    Assert-GameRunning
    [IO.File]::AppendAllText($pad, "$($button):150`n")
    Wait-Game (0.2 + $after)
}
function Qpc-Milliseconds { [Diagnostics.Stopwatch]::GetTimestamp() * 1000.0 / [Diagnostics.Stopwatch]::Frequency }
function Measure-Window([string]$name, [int]$seconds) {
    $p.Refresh()
    $cpuStart = $p.TotalProcessorTime.TotalMilliseconds
    $begin = Qpc-Milliseconds
    Wait-Game $seconds
    $end = Qpc-Milliseconds
    $p.Refresh()
    $script:windows += @{ name=$name; start=$begin; end=$end
        cpu_ms=$p.TotalProcessorTime.TotalMilliseconds - $cpuStart
        logical_processors=[Environment]::ProcessorCount }
}
try {
    foreach ($key in $envs.Keys) {
        $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $envs[$key], 'Process')
    }
    $p = Start-Process -FilePath $Exe -WorkingDirectory $repo -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError $log
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
    @{ executable=$Exe; sha256=(Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash
       process_id=$p.Id; profile=$Profile; scenario=$Scenario; settings=$envs; explicit_position=$explicitPosition; coordinates=@($X, $Z); heading=$Heading
       save_template=$Save; shader_cache_seed=$ShaderCacheSeed; logical_processors=[Environment]::ProcessorCount
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $dir 'run.json')
    Write-Output "Game PID $($p.Id), evidence $dir"
    $intro = [Diagnostics.Stopwatch]::StartNew()
    $frontendReady = $false
    while ($intro.Elapsed.TotalSeconds -lt 120) {
        Wait-Game 0.7
        # Movie-only presentations do not emit guest-flip CSV rows. Detect
        # frontend rendering without requiring the optional movie-trace build.
        if ((Test-Path -LiteralPath $csv) -and (Get-Item -LiteralPath $csv).Length -gt 128) {
            $frontendReady = $true; break
        }
        Press-Pad 'start' 0
    }
    if (-not $frontendReady) { throw 'No frontend guest flips before the startup timeout.' }
    Wait-Game 5
    Press-Pad 'start' 4
    Press-Pad 'a' 6
    Measure-Window 'frontend' 10
    foreach ($step in @(@('down', 1), @('a', 4), @('a', 4), @('a', 5), @('a', 8), @('a', 10), @('a', 9), @('start', 6))) {
        Press-Pad $step[0] $step[1]
    }
    $position = $null
    for ($attempt = 0; $attempt -lt 5; $attempt++) {
        Assert-GameRunning
        try { $position = & $teleport -ProcessId $p.Id } catch { $position = $null }
        if ($position -and $position.available) { break }
        Press-Pad 'a' 9
    }
    if (-not $position -or -not $position.available) { throw 'No live player car; race benchmark is invalid.' }
    $position | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir 'position-start.json')
    if ($explicitPosition) {
        $position = & $teleport -ProcessId $p.Id -X $X -Z $Z -Heading $Heading
    }
    $position | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir 'position.json')
    Wait-Game 4
    if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) {
        $gpuProcess = Start-Process -FilePath nvidia-smi -WindowStyle Hidden -PassThru `
            -ArgumentList @('--query-gpu=utilization.gpu,power.draw', '--format=csv,noheader,nounits', '-lms', '500') `
            -RedirectStandardOutput (Join-Path $dir 'gpu.csv') -RedirectStandardError (Join-Path $dir 'gpu-errors.txt')
    }
    # A fixed throttle input exercises streaming, traffic and effects. It is
    # a repeatable input, not a deterministic route: collisions can differ.
    $measurePosition = & $teleport -ProcessId $p.Id
    $measurePosition | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir 'position-measure-start.json')
    if ($Scenario -eq 'Driving') {
        [IO.File]::AppendAllText($pad, "rt:$($Secs * 1000 + 500)`n")
        Wait-Game 0.25
    }
    Measure-Window 'race' $Secs
    $endPosition = & $teleport -ProcessId $p.Id
    $endPosition | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir 'position-end.json')
    if ($Scenario -eq 'Driving' -and (-not $measurePosition.available -or -not $endPosition.available -or
        [Math]::Pow($endPosition.x - $measurePosition.x, 2) +
        [Math]::Pow($endPosition.z - $measurePosition.z, 2) -lt 1)) {
        throw 'Driving benchmark did not move the player car by at least one metre.'
    }
    # The CSV flushes every 120 flips. Wait for persisted evidence covering
    # the complete window, including the 30 FPS profile, before stopping.
    $flushTimer = [Diagnostics.Stopwatch]::StartNew()
    $persisted = $false
    while ($flushTimer.Elapsed.TotalSeconds -lt 15) {
        Wait-Game 0.25
        $lastRow = Get-Content -LiteralPath $csv -Tail 1
        $lastTimestamp = 0.0
        if ($lastRow -and [double]::TryParse(($lastRow -split ',')[0],
            [Globalization.NumberStyles]::Float, [cultureinfo]::InvariantCulture, [ref]$lastTimestamp) -and
            $lastTimestamp -ge $windows[-1].end) { $persisted = $true; break }
    }
    if (-not $persisted) { throw 'Frame CSV did not persist the complete measurement window.' }
    $windows | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir 'windows.json')
} finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
    if ($gpuProcess -and -not $gpuProcess.HasExited) { Stop-Process -Id $gpuProcess.Id -Force }
    if ($p) {
        if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
        $p.WaitForExit(10000) | Out-Null
    }
    $resolvedSave = [IO.Path]::GetFullPath($runSave)
    if ($resolvedSave -ne [IO.Path]::GetFullPath((Join-Path $dir 'save')) -or
        -not $resolvedSave.StartsWith($benchRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to remove a save outside this benchmark run.'
    }
    Remove-Item -LiteralPath $resolvedSave -Recurse -Force -ErrorAction SilentlyContinue
}
$python = Join-Path $repo 'tools\venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $python)) { $python = 'python' }
& $python (Join-Path $PSScriptRoot 'summarize.py') $dir
if ($LASTEXITCODE -ne 0) { throw 'Benchmark summary failed; see retained evidence.' }
