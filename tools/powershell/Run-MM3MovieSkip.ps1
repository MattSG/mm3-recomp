<#
.SYNOPSIS
Run MM3 with real emulated USB presses to shorten intro playback.
.DESCRIPTION
Requires an executable built with MM3_MOVIE_INPUT_TRACE=ON. The optional
wrappers observe movie boundaries and the game's own skip decision; they never
change guest state. Live mode presses only during selected movies and stops
pressing once they close. Timed mode repeats a fixed USB timeline.
Each run has separate save data and logs under conformance_tmp. Only one recomp
instance is allowed, and this harness stops its own process at the time limit.
TracePostMovieFrame requests one GPU frame trace five seconds after intro.bik
closes; set RECOMP_PB_EXEC=1 to execute and observe the GPU command stream.
.EXAMPLE
pwsh tools/powershell/Run-MM3MovieSkip.ps1
.EXAMPLE
pwsh tools/powershell/Run-MM3MovieSkip.ps1 -Headless -SkipMovies intro.bik
.EXAMPLE
pwsh tools/powershell/Run-MM3MovieSkip.ps1 -Headless -InputMode Timed
#>
param(
    [string]$Executable = 'build-msvc-tailfix/movie-input/RelWithDebInfo/mm3_recomp.exe',
    [ValidateSet('Live', 'Timed')][string]$InputMode = 'Live',
    [ValidateSet('start', 'a', 'b')][string]$Button = 'start',
    [ValidateSet('dice.bik', 'msgs.bik', 'intro.bik', 'AttractMode2.bik')]
    [string[]]$SkipMovies = @('dice.bik', 'msgs.bik', 'intro.bik'),
    [ValidateRange(1, 600)][int]$DurationSeconds = 45,
    [switch]$Headless,
    [switch]$TracePostMovieFrame
)

$ErrorActionPreference = 'Stop'
function Set-RunEnvironment([string]$Name, $Value) {
    # Preserve a real null through PowerShell's string argument conversion.
    # An empty environment value still enables C flags tested with getenv.
    $text = if ($null -eq $Value) { [NullString]::Value } else { [string]$Value }
    [Environment]::SetEnvironmentVariable($Name, $text, 'Process')
}
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$exe = if ([IO.Path]::IsPathRooted($Executable)) { $Executable } else { Join-Path $repo $Executable }
if (-not (Test-Path -LiteralPath $exe)) { throw "Executable missing: $exe" }
if (Get-Process -Name mm3_recomp -ErrorAction SilentlyContinue) {
    throw 'Stop the existing recomp instance before launching another.'
}
if ($InputMode -eq 'Timed' -and
    (@($SkipMovies).Count -ne 3 -or @('dice.bik', 'msgs.bik', 'intro.bik' | Where-Object { $_ -notin $SkipMovies }).Count)) {
    throw 'Use Live mode to target individual movies; Timed mode presses on a fixed timeline.'
}
$run = Join-Path $repo ('conformance_tmp/movie_skip_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
New-Item -ItemType Directory -Path (Join-Path $run 'save/Cache') -Force | Out-Null
$live = Join-Path $run 'pad.txt'
[IO.File]::WriteAllText($live, '')
$log = Join-Path $run 'stderr.log'
$settings = @{
    RECOMP_USB = '1'; RECOMP_INPUT_DIAG = '1'; RECOMP_USB_STATS = '1'
    MM3_SAVE_DIR = (Join-Path $run 'save')
    RECOMP_PAD_PRESS = $null; RECOMP_PAD_SCRIPT = $null; RECOMP_PAD_LIVE = $null
    RECOMP_FB_WINDOW = $(if ($Headless) { $null } else { '1' })
}
if ($TracePostMovieFrame) {
    $settings.RECOMP_FRAME_TRACE = (Join-Path $run 'frame.flag')
    $settings.RECOMP_FRAME_TRACE_METHODS = $null
}
if ($InputMode -eq 'Live') { $settings.RECOMP_PAD_LIVE = $live }
else {
    $settings.RECOMP_PAD_SCRIPT = ((0..([Math]::Min(255, $DurationSeconds * 2)) |
        ForEach-Object { '{0}:{1}:200' -f (500 + $_ * 500), $Button }) -join ',')
}
$previous = @{}
try {
    foreach ($key in $settings.Keys) {
        $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        Set-RunEnvironment $key $settings[$key]
    }
    $launch = @{
        FilePath = $exe; WorkingDirectory = $repo; PassThru = $true
        WindowStyle = $(if ($Headless) { 'Hidden' } else { 'Normal' })
        RedirectStandardOutput = (Join-Path $run 'stdout.log'); RedirectStandardError = $log
    }
    $process = Start-Process @launch
} finally {
    foreach ($key in $previous.Keys) {
        Set-RunEnvironment $key $previous[$key]
    }
}
Write-Output "PID=$($process.Id) evidence=$run"
$clock = [Diagnostics.Stopwatch]::StartNew()
$activeMovie = ''
$lastPress = -1000
$reader = $null
$pending = ''
$frameTraceAt = [long]::MaxValue
try {
    $stream = [IO.File]::Open($log, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    $reader = [IO.StreamReader]::new($stream)
    while (-not $process.HasExited -and $clock.Elapsed.TotalSeconds -lt $DurationSeconds) {
        $parts = ($pending + $reader.ReadToEnd()) -split "`n"
        $pending = $parts[-1]
        for ($i = 0; $i -lt $parts.Count - 1; $i++) {
            $line = $parts[$i]
            if ($line -match '\[MOVIE_FRAME\] total=(\d+) frame=(\d+)') {
                $activeMovie = switch ([int]$Matches[1]) {
                    144 { 'dice.bik' }; 308 { 'msgs.bik' }; 3114 { 'intro.bik' }; 3092 { 'AttractMode2.bik' }; default { '' }
                }
            }
            if ($line -match '\[MOVIE_END\]') {
                $activeMovie = ''; Write-Output $line
                if ($TracePostMovieFrame -and $line -match 'name=intro.bik') {
                    $frameTraceAt = $clock.ElapsedMilliseconds + 5000
                }
            }
            if ($line -match '\[MOVIE_SKIP_INPUT\]|\[CRASH\]') { Write-Output $line }
        }
        if ($clock.ElapsedMilliseconds -ge $frameTraceAt) {
            [IO.File]::WriteAllText($settings.RECOMP_FRAME_TRACE, 'capture')
            $frameTraceAt = [long]::MaxValue
        }
        if ($InputMode -eq 'Live' -and $activeMovie -in $SkipMovies -and
            $clock.ElapsedMilliseconds - $lastPress -ge 500) {
            [IO.File]::AppendAllText($live, "$($Button):200`n")
            $lastPress = $clock.ElapsedMilliseconds
        }
        Start-Sleep -Milliseconds 100
        $process.Refresh()
    }
} finally {
    if ($reader) { $reader.Dispose() }
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
# Timed mode is deliberately a fixed repeating timeline; use Live mode to
# target selected movies. Both paths generate USB reports, never desktop keys.
