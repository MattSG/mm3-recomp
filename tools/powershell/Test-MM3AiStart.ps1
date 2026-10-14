<#
.SYNOPSIS
Check that the AI drives off the start line forwards in the first race.

.DESCRIPTION
  pwsh tools\powershell\Test-MM3AiStart.ps1 -Exe <mm3_recomp.exe> -Save <save template> [-FpsLimit 60] [-Secs 20]

Takes the bench's menu path into a race (tools\bench\run.ps1), holds throttle
on the player's car, and samples every car through the MM3_DEBUG_TELEPORT pipe
("cars": X/Z plus the body's forward vector) every 250 ms. Per car it sums the
distance travelled along its own forward vector and against it. The player's
car is the sign check: under throttle it must come out forwards.

Verdict FAIL when any AI car covers more than 10 units backwards or an AI car
ends up behind where it started. Exit code 0 = PASS, 1 = FAIL, 2 = no race.
#>
param([Parameter(Mandatory)][string]$Exe, [Parameter(Mandatory)][string]$Save,
      [string]$Tag = 'ai-start', [int]$FpsLimit = 60, [int]$Secs = 20, [switch]$Course, [switch]$Crash, [string]$CrashAt)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dir = Join-Path $repo "conformance_tmp\ai_start\$Tag"
Get-Process mm3_recomp -ErrorAction SilentlyContinue | ForEach-Object { $_.Kill(); $_.WaitForExit(15000) | Out-Null }
if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
New-Item -ItemType Directory -Force $dir | Out-Null
Copy-Item -Recurse $Save (Join-Path $dir 'save')
$pad = Join-Path $dir 'pad.txt'; [IO.File]::WriteAllText($pad, '')
$log = Join-Path $dir 'stderr.log'
$envs = @{ RECOMP_PB_EXEC='1'; RECOMP_FB_WINDOW='1'; RECOMP_USB='1'; RECOMP_PAD_LIVE=$pad
           RECOMP_APU_DSP_ACK='0x80458810'; MM3_SAVE_DIR=(Join-Path $dir 'save')
           RECOMP_FPS_LIMIT="$FpsLimit"; MM3_DEBUG_TELEPORT='1'
           RECOMP_FRAME_CSV=(Join-Path $dir 'frames.csv') }
if ($env:MM3_CRASH_CAPTURE) { $envs.RECOMP_PRESENT_CAPTURE = $env:MM3_CRASH_CAPTURE }
foreach ($k in $envs.Keys) { Set-Item "Env:$k" $envs[$k] }
$p = Start-Process -FilePath $Exe -WorkingDirectory $repo -PassThru `
     -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError $log
foreach ($k in $envs.Keys) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }

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
    while ($sw.Elapsed.TotalSeconds -lt 120) {
        Start-Sleep -Milliseconds 700
        $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
        if ($t -match 'MOVIE_END\] name=intro') { break }
        if ($t -match 'MOVIE_FRAME') { press 'start' 0 }
    }
    Start-Sleep 5
    press 'start' 4; press 'a' 6
    foreach ($s in @(@('down', 1), @('a', 4), @('a', 4), @('a', 5), @('a', 8), @('a', 10), @('a', 9), @('start', 6))) { press $s[0] $s[1] }
    $live = $null
    for ($i = 0; $i -lt 6 -and -not $live; $i++) {
        $live = cars | Where-Object player
        if (-not $live) { press 'a' 9 }
    }
    if (-not $live) { Write-Host 'NO RACE: player car never became live'; exit 2 }

    # Hold throttle (right trigger) through the window; sample everyone.
    $samples = @{}
    $end = [Diagnostics.Stopwatch]::StartNew()
    # One unbroken hold: repeated presses leave 200 ms releases between them.
    # -Course instead: throttle 6 s, brake 3 s, throttle 4 s, throttle + full
    # left 1 s, throttle to the end. -Crash: throttle 4 s, throttle + full
    # right 0.6 s (into the buildings), throttle to the end.
    $steps = @()
    if ($Course) {
        [IO.File]::AppendAllText($pad, "rt:6000`n")
        $steps = @(@(6000, 'lt:3000'), @(9000, 'rt:4000'), @(13000, 'rt+lleft:1000'), @(14000, "rt:$($Secs * 1000)"))
    } elseif ($CrashAt) {
        # The same wall every run: teleported to x,z facing heading (radians),
        # then throttle into it.
        $x, $z, $h = $CrashAt.Split(',') | ForEach-Object { [double]$_ }
        $pipe = [IO.Pipes.NamedPipeClientStream]::new('.', "MM3Teleport-$($p.Id)", [IO.Pipes.PipeDirection]::InOut)
        $pipe.Connect(3000)
        $cmd = [Text.Encoding]::ASCII.GetBytes([string]::Format([cultureinfo]::InvariantCulture, 'teleport {0} {1} {2}', $x, $z, $h))
        $pipe.Write($cmd, 0, $cmd.Length); $buf = [byte[]]::new(512); [void]$pipe.Read($buf, 0, 512); $pipe.Dispose()
        Start-Sleep 2
        $end.Restart()
        [IO.File]::AppendAllText($pad, "rt:$($Secs * 1000)`n")
    } elseif ($Crash) {
        [IO.File]::AppendAllText($pad, "rt:4000`n")
        $steps = @(@(4000, 'rt+lright:600'), @(4600, "rt:$($Secs * 1000)"))
    } else {
        [IO.File]::AppendAllText($pad, "rt:$($Secs * 1000 + 1000)`n")
    }
    $ci = 0
    while ($end.Elapsed.TotalSeconds -lt $Secs) {
        if ($CrashAt -and $env:MM3_CRASH_CAPTURE -and -not $capped -and $end.Elapsed.TotalSeconds -ge 1.2) {
            # Frames of the approach, to see what touches the car.
            [IO.File]::WriteAllText("$env:MM3_CRASH_CAPTURE.flag", '200'); $capped = $true
        }
        if ($ci -lt $steps.Count -and $end.Elapsed.TotalMilliseconds -ge $steps[$ci][0] - 150) {
            [IO.File]::AppendAllText($pad, "$($steps[$ci][1])`n"); $ci++
        }
        foreach ($c in cars) {
            if (-not $samples.ContainsKey($c.id)) { $samples[$c.id] = [Collections.ArrayList]::new() }
            [void]$samples[$c.id].Add(($c | Add-Member -PassThru t $end.Elapsed.TotalSeconds))
        }
        Start-Sleep -Milliseconds 250
    }
} finally {
    if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit(15000) | Out-Null }
    Remove-Item -Recurse -Force (Join-Path $dir 'save') -ErrorAction SilentlyContinue   # 5 GB per run
}

$fail = $false
foreach ($id in $samples.Keys) {   # every car's track, for a closer look
    $samples[$id] | Select-Object t, x, z, fx, fz, player | Export-Csv (Join-Path $dir "car-$id.csv") -NoTypeInformation
}
$rows = foreach ($id in $samples.Keys) {
    $s = $samples[$id]; $fwd = 0.0; $back = 0.0
    for ($i = 1; $i -lt $s.Count; $i++) {
        $along = ($s[$i].x - $s[$i-1].x) * $s[$i-1].fx + ($s[$i].z - $s[$i-1].z) * $s[$i-1].fz
        if ($along -ge 0) { $fwd += $along } else { $back -= $along }
    }
    $net = ($s[-1].x - $s[0].x) * $s[0].fx + ($s[-1].z - $s[0].z) * $s[0].fz
    $player = [bool]$s[0].player
    $bad = -not $player -and ($back -gt 10 -or $net -lt 0)
    if ($bad) { $fail = $true }
    [pscustomobject]@{ car = $(if ($player) { "player" } else { "ai $id" }); vt = "{0:X8}" -f [uint32]$s[0].vt; samples = $s.Count
        forward = [math]::Round($fwd, 1); backward = [math]::Round($back, 1)
        net = [math]::Round($net, 1); verdict = $(if ($bad) { 'REVERSING' } else { 'ok' }) }
}
$rows | Sort-Object car | Format-Table -AutoSize | Out-String | Write-Host
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
# Acceleration: path length covered by 2/4/6/8/12 s of throttle. Same physics
# at every frame rate means the same curve.
if ($ps.Count -gt 1) {
    $d = 0.0; $marks = @(2, 4, 6, 8, 12); $mi = 0; $out = @()
    for ($i = 1; $i -lt $ps.Count -and $mi -lt $marks.Count; $i++) {
        $d += [math]::Sqrt([math]::Pow($ps[$i].x - $ps[$i-1].x, 2) + [math]::Pow($ps[$i].z - $ps[$i-1].z, 2))
        while ($mi -lt $marks.Count -and $ps[$i].t - $ps[0].t -ge $marks[$mi]) { $out += ("{0}s={1:N0}" -f $marks[$mi], $d); $mi++ }
    }
    Write-Host ("player distance: " + ($out -join ' '))
}
if ($Course -and $ps.Count -gt 1) {
    # Braking: from 6 s, distance rolled until the first sample under 1
    # unit/s (held on, the brake reverses). Steering: heading turned and path
    # covered from 13 s to 14 s.
    function at([double]$s) { $ps | Where-Object { $_.t - $ps[0].t -ge $s } | Select-Object -First 1 }
    function path([double]$a, [double]$b) {
        $d = 0.0
        for ($i = 1; $i -lt $ps.Count; $i++) {
            $t = $ps[$i].t - $ps[0].t
            if ($t -gt $a -and $t -le $b) {
                $d += [math]::Sqrt([math]::Pow($ps[$i].x - $ps[$i-1].x, 2) + [math]::Pow($ps[$i].z - $ps[$i-1].z, 2))
            }
        }
        $d
    }
    $v6 = (path 5.5 6) / 0.5; $roll = 0.0; $stop = $null
    for ($i = 1; $i -lt $ps.Count -and -not $stop; $i++) {
        $t = $ps[$i].t - $ps[0].t
        if ($t -le 6) { continue }
        $step = [math]::Sqrt([math]::Pow($ps[$i].x - $ps[$i-1].x, 2) + [math]::Pow($ps[$i].z - $ps[$i-1].z, 2))
        $roll += $step
        if ($step / ($ps[$i].t - $ps[$i-1].t) -lt 1) { $stop = $t }
    }
    Write-Host ("braking: from {0:N1} units/s, rolled {1:N1} units, stopped {2}" -f $v6, $roll,
        $(if ($stop) { "after {0:N2} s" -f ($stop - 6) } else { 'never' }))
    $h0 = at 13; $h1 = at 14.25
    if ($h0 -and $h1) {
        $turn = ([math]::Atan2($h1.fx, $h1.fz) - [math]::Atan2($h0.fx, $h0.fz)) * 180 / [math]::PI
        while ($turn -gt 180) { $turn -= 360 }; while ($turn -lt -180) { $turn += 360 }
        Write-Host ("steering: from {0:N1} units/s turned {1:N0} deg over {2:N1} units" -f ((path 12.5 13) / 0.5), $turn, (path 13 14.25))
    }
    $ai = foreach ($id in $samples.Keys) {
        $s = $samples[$id]
        if (-not $s[0].player) {
            $d = 0.0
            for ($i = 1; $i -lt $s.Count; $i++) { $d += [math]::Sqrt([math]::Pow($s[$i].x - $s[$i-1].x, 2) + [math]::Pow($s[$i].z - $s[$i-1].z, 2)) }
            $d
        }
    }
    Write-Host ("ai path over {0} s: {1}" -f $Secs, (($ai | Sort-Object | ForEach-Object { "{0:N0}" -f $_ }) -join ' '))
}
if (($Crash -or $CrashAt) -and $ps.Count -gt 2) {
    # The first impact after 4 s: a sample interval in which speed falls by
    # more than half from over 10 units/s. Speed before, after, and 2 s on.
    $v = @(0.0) + @(for ($i = 1; $i -lt $ps.Count; $i++) {
        [math]::Sqrt([math]::Pow($ps[$i].x - $ps[$i-1].x, 2) + [math]::Pow($ps[$i].z - $ps[$i-1].z, 2)) / ($ps[$i].t - $ps[$i-1].t) })
    $hit = $null
    for ($i = 2; $i -lt $ps.Count -and -not $hit; $i++) {
        if ($ps[$i].t - $ps[0].t -gt $(if ($CrashAt) { 0.5 } else { 4 }) -and $v[$i-1] -gt 10 -and $v[$i] -lt $v[$i-1] / 2) { $hit = $i }
    }
    if ($hit) {
        $j = $hit; while ($j -lt $ps.Count - 1 -and $ps[$j].t - $ps[$hit].t -lt 2) { $j++ }
        $k = $hit; while ($k -gt 0 -and $ps[$hit].t - $ps[$k].t -lt 1.5) { $k-- }
        Write-Host ("crash: at {0:N1} s, {1:N1} -> {2:N1} units/s, {3:N1} units/s 2 s later" -f ($ps[$hit].t - $ps[0].t), $v[$hit-1], $v[$hit], $v[$j])
        Write-Host ("crash: 1.5 s before at x={0:R} z={1:R} heading={2:R}" -f $ps[$k].x, $ps[$k].z, [math]::Atan2($ps[$k].fx, $ps[$k].fz))
    } else { Write-Host 'crash: no impact' }
    if ($CrashAt) {
        # Into a wall: peak speed, when it stops, where it rests (a bounce
        # shows as resting short of the furthest point).
        $pk = 0; for ($i = 1; $i -lt $v.Count; $i++) { if ($v[$i] -gt $v[$pk]) { $pk = $i } }
        $st = $pk; while ($st -lt $v.Count - 1 -and $v[$st] -ge 1) { $st++ }
        $far = ($ps | ForEach-Object { $_.z } | Measure-Object -Minimum).Minimum
        Write-Host ("wall: peak {0:N1} units/s at {1:N2} s, stopped at {2:N2} s, rests z={3:N2} (furthest {4:N2})" -f $v[$pk],
            ($ps[$pk].t - $ps[0].t), ($ps[$st].t - $ps[0].t), $ps[-1].z, $far)
    }
}
if (-not $playerRow -or $playerRow.forward -le $playerRow.backward) {
    Write-Host 'WARNING: player car did not come out forwards under throttle; forward vector sign is suspect'
}
# The frame rate the race actually ran at: median of the last $Secs seconds of flips.
$ft = @(Get-Content (Join-Path $dir 'frames.csv') -ErrorAction SilentlyContinue | Select-Object -Skip 1 |
        ForEach-Object { [double]($_ -split ',')[1] })
$n = [math]::Min($ft.Count, [int]($Secs * 60))
if ($n -gt 0) {
    $med = ($ft[($ft.Count - $n)..($ft.Count - 1)] | Sort-Object)[[int]($n / 2)]
    if ($med -gt 0) { Write-Host ("race frame rate: {0:N0} fps (median)" -f (1000.0 / $med)) }
}
Write-Host ("fps limit {0}: {1}" -f $FpsLimit, $(if ($fail) { 'FAIL' } else { 'PASS' }))
exit $(if ($fail) { 1 } else { 0 })
