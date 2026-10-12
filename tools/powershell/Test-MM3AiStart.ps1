<#
.SYNOPSIS
Check that the AI drives forwards in the selected race or mission.

.DESCRIPTION
  pwsh tools\powershell\Test-MM3AiStart.ps1 -Exe <mm3_recomp.exe> -Save <save template> [-FpsLimit 60] [-Secs 20]

Takes the bench's menu path into a race (tools\bench\run.ps1), holds throttle
on the player's car, and samples every car through the MM3_DEBUG_TELEPORT pipe
("cars": X/Z plus the body's forward vector) every 250 ms. Per car it sums the
distance travelled along its own forward vector and against it. The player's
car is the sign check: under throttle it must come out forwards.

Verdict FAIL when any AI car covers more than 10 units backwards along its own
forward vector. Net displacement is diagnostic: a normal turn can finish behind
the initial heading without driving backwards. Cars need at least three samples.
The player must have at least three samples
and travel at least one unit more forwards than backwards under throttle.
Exit code 0 = PASS, 1 = FAIL, 2 = no race.
#>
param([Parameter(Mandatory)][string]$Exe, [Parameter(Mandatory)][string]$Save,
      [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$')][string]$Tag = 'ai-start',
      [ValidateRange(1, 90)][int]$FpsLimit = 60, [ValidateRange(5, 300)][int]$Secs = 20,
      [switch]$Capture)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $repo 'conformance_tmp\ai_start'))
$dir = [IO.Path]::GetFullPath((Join-Path $testRoot $Tag))
if (-not $dir.StartsWith($testRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Test tag must stay inside the AI test root.' }
if (Get-Process mm3_recomp -ErrorAction SilentlyContinue) { throw 'Close the running game before starting an isolated AI test.' }
if ([IO.Path]::GetFullPath($Save).StartsWith($dir.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The test output must not contain its save template.'
}
if (Test-Path $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
New-Item -ItemType Directory -Force $dir | Out-Null
Copy-Item -Recurse $Save (Join-Path $dir 'save')
$pad = Join-Path $dir 'pad.txt'; [IO.File]::WriteAllText($pad, '')
$log = Join-Path $dir 'stderr.log'
$envs = @{ RECOMP_PB_EXEC='1'; RECOMP_FB_WINDOW='1'; RECOMP_USB='1'; RECOMP_PAD_LIVE=$pad
           RECOMP_APU_DSP_ACK='0x80458810'; MM3_SAVE_DIR=(Join-Path $dir 'save')
           RECOMP_FPS_LIMIT="$FpsLimit"; MM3_DEBUG_TELEPORT='1'; RECOMP_PAUSE_UNFOCUSED='0'
           RECOMP_FRAME_CSV=(Join-Path $dir 'frames.csv') }
if ($Capture) { $envs.RECOMP_PRESENT_CAPTURE = Join-Path $dir 'capture' }
$previous = @{}
try {
    foreach ($k in $envs.Keys) {
        $previous[$k] = [Environment]::GetEnvironmentVariable($k, 'Process')
        [Environment]::SetEnvironmentVariable($k, $envs[$k], 'Process')
    }
    $p = Start-Process -FilePath $Exe -WorkingDirectory $repo -PassThru `
         -WindowStyle Hidden -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError $log
} finally {
    foreach ($k in $previous.Keys) { [Environment]::SetEnvironmentVariable($k, $previous[$k], 'Process') }
}

function press([string]$b, [double]$after, [int]$ms = 150) {
    [IO.File]::AppendAllText($pad, "$($b):$ms`n"); Start-Sleep -Milliseconds 100; Start-Sleep $after }
function cars {
    $pipe = [IO.Pipes.NamedPipeClientStream]::new('.', "MM3Teleport-$($p.Id)", [IO.Pipes.PipeDirection]::InOut)
    try {
        $pipe.Connect(3000)
        $cmd = [Text.Encoding]::ASCII.GetBytes('cars'); $pipe.Write($cmd, 0, $cmd.Length)
        $buf = [byte[]]::new(4096); $read = $pipe.ReadAsync($buf, 0, $buf.Length)
        if (-not $read.Wait(5000)) { return @() }
        @(([Text.Encoding]::ASCII.GetString($buf, 0, $read.Result) | ConvertFrom-Json).cars)
    } catch { @() } finally { $pipe.Dispose() }
}

try {
    # Same path as the bench: skip intro movies, title -> profile -> menu -> race.
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $ready = $false
    while ($sw.Elapsed.TotalSeconds -lt 120) {
        Start-Sleep -Milliseconds 700
        if ($p.HasExited) { throw "Game exited during startup with code $($p.ExitCode)." }
        $csv = Join-Path $dir 'frames.csv'
        if ((Test-Path -LiteralPath $csv) -and (Get-Item -LiteralPath $csv).Length -gt 128) {
            $ready = $true; break
        }
        press 'start' 0
    }
    if (-not $ready) { throw 'No frontend guest flips before the startup timeout.' }
    Start-Sleep 5
    press 'start' 4; press 'a' 6
    if ($Capture) { [IO.File]::WriteAllText((Join-Path $dir 'capture.flag'), '1') }
    foreach ($s in @(@('down', 1), @('a', 4), @('a', 4), @('a', 5), @('a', 8), @('a', 10), @('a', 9), @('start', 6))) { press $s[0] $s[1] }
    $live = $null
    for ($i = 0; $i -lt 6 -and -not $live; $i++) {
        $live = cars | Where-Object player
        if (-not $live) { press 'a' 9 }
    }
    if (-not $live) { Write-Host 'NO RACE: player car never became live'; exit 2 }
    if ($Capture) { [IO.File]::WriteAllText((Join-Path $dir 'capture.flag'), '1') }

    # Hold throttle (right trigger) through the window; sample everyone.
    $samples = @{}
    $end = [Diagnostics.Stopwatch]::StartNew()
    # One unbroken hold: repeated presses leave 200 ms releases between them.
    [IO.File]::AppendAllText($pad, "rt:$($Secs * 1000 + 1000)`n")
    while ($end.Elapsed.TotalSeconds -lt $Secs) {
        foreach ($c in cars) {
            if (-not $samples.ContainsKey($c.id)) { $samples[$c.id] = [Collections.ArrayList]::new() }
            [void]$samples[$c.id].Add(($c | Add-Member -PassThru t $end.Elapsed.TotalSeconds))
        }
        Start-Sleep -Milliseconds 250
    }
} finally {
    if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit(15000) | Out-Null }
    $testSave = [IO.Path]::GetFullPath((Join-Path $dir 'save'))
    if (-not $testSave.StartsWith($testRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Test save escaped the AI test root.' }
    Remove-Item -LiteralPath $testSave -Recurse -Force -ErrorAction SilentlyContinue
}

$fail = $false
$playerSeen = $false
$aiSeen = $false
$playerFailures = [Collections.Generic.List[string]]::new()
$rows = foreach ($id in $samples.Keys) {
    $s = $samples[$id]; $fwd = 0.0; $back = 0.0
    for ($i = 1; $i -lt $s.Count; $i++) {
        $along = ($s[$i].x - $s[$i-1].x) * $s[$i-1].fx + ($s[$i].z - $s[$i-1].z) * $s[$i-1].fz
        if ($along -ge 0) { $fwd += $along } else { $back -= $along }
    }
    $net = ($s[-1].x - $s[0].x) * $s[0].fx + ($s[-1].z - $s[0].z) * $s[0].fz
    $player = [bool]$s[0].player
    $bad = -not $player -and $back -gt 10
    $verdict = if ($bad) { 'REVERSING' } else { 'ok' }
    if (-not $player) {
        $aiSeen = $true
        if ($s.Count -lt 3) { $bad = $true; $verdict = 'INSUFFICIENT SAMPLES' }
    }
    if ($player) {
        $playerSeen = $true
        if ($s.Count -lt 3) {
            $bad = $true
            $verdict = 'INSUFFICIENT SAMPLES'
            $playerFailures.Add("Player telemetry has only $($s.Count) samples; need at least 3.")
        } elseif ($fwd -le $back -or $fwd - $back -lt 1.0) {
            $bad = $true
            $verdict = 'NOT ACCELERATING'
            $playerFailures.Add(('Player did not move meaningfully forwards under throttle (forward {0:F3}, backward {1:F3} units).' -f $fwd, $back))
        }
    }
    if ($bad) { $fail = $true }
    [pscustomobject]@{ car = $(if ($player) { "player" } else { "ai $id" }); vt = "{0:X8}" -f [uint32]$s[0].vt; samples = $s.Count
        forward = [math]::Round($fwd, 1); backward = [math]::Round($back, 1)
        net = [math]::Round($net, 1); verdict = $verdict }
}
$rows | Sort-Object car | Format-Table -AutoSize | Out-String | Write-Host
$sampleRows = @(foreach ($id in $samples.Keys) {
    [pscustomobject]@{ id = $id; samples = @($samples[$id]) }
})
$sampleRows | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $dir 'samples.json')
$rows | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir 'cars.json')
if (-not $playerSeen) {
    $fail = $true
    $playerFailures.Add('No player telemetry samples were collected under throttle.')
}
if (-not $aiSeen) {
    $fail = $true
    $playerFailures.Add('No AI car telemetry was collected; AI direction is unverified.')
}
foreach ($reason in $playerFailures) { Write-Host "FAIL: $reason" }
$playerRow = $rows | Where-Object car -eq 'player'
# Throttle/gear check: top speed over any 1 s of the player's samples (units/s).
$ps = foreach ($id in $samples.Keys) { if ($samples[$id][0].player) { $samples[$id] } }
$top = 0.0
for ($i = 0; $i -lt $ps.Count; $i++) {
    for ($j = $i + 1; $j -lt $ps.Count -and $ps[$j].t - $ps[$i].t -lt 1; $j++) {}
    if ($j -lt $ps.Count) {
        $v = [math]::Sqrt([math]::Pow($ps[$j].x - $ps[$i].x, 2) + [math]::Pow($ps[$j].z - $ps[$i].z, 2)) / ($ps[$j].t - $ps[$i].t)
        if ($v -gt $top) { $top = $v }
    }
}
Write-Host ("player top speed: {0:N1} units/s" -f $top)
if (-not $playerRow -or $playerRow.forward -le $playerRow.backward) {
    Write-Host 'WARNING: player car did not come out forwards under throttle; forward vector sign is suspect'
}
# The observed rate uses elapsed timestamps, regardless of the requested cap.
$frameRows = @(Import-Csv -LiteralPath (Join-Path $dir 'frames.csv') -ErrorAction SilentlyContinue |
    ForEach-Object {
        $timestamp = 0.0; $frameMs = 0.0
        if ([double]::TryParse($_.qpc_ms, [Globalization.NumberStyles]::Float,
                [Globalization.CultureInfo]::InvariantCulture, [ref]$timestamp) -and
            [double]::TryParse($_.frame_ms, [Globalization.NumberStyles]::Float,
                [Globalization.CultureInfo]::InvariantCulture, [ref]$frameMs) -and
            -not [double]::IsNaN($timestamp) -and -not [double]::IsInfinity($timestamp) -and
            -not [double]::IsNaN($frameMs) -and -not [double]::IsInfinity($frameMs) -and $frameMs -gt 0) {
            [pscustomobject]@{ qpc_ms = $timestamp; frame_ms = $frameMs }
        }
    })
if ($frameRows.Count -gt 0) {
    $lastTimestamp = ($frameRows | Measure-Object -Property qpc_ms -Maximum).Maximum
    $ft = @($frameRows | Where-Object { $_.qpc_ms -ge $lastTimestamp - $Secs * 1000 } |
        Select-Object -ExpandProperty frame_ms | Sort-Object)
    $middle = [int][math]::Floor($ft.Count / 2)
    $med = if ($ft.Count % 2) { $ft[$middle] } else { ($ft[$middle - 1] + $ft[$middle]) / 2 }
    Write-Host ("race frame rate: {0:N0} fps (median)" -f (1000.0 / $med))
}
Write-Host ("fps limit {0}: {1}" -f $FpsLimit, $(if ($fail) { 'FAIL' } else { 'PASS' }))
exit $(if ($fail) { 1 } else { 0 })
