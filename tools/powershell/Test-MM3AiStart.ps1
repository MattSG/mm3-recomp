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
      [string]$Tag = 'ai-start', [int]$FpsLimit = 60, [int]$Secs = 20)
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
           RECOMP_FPS_LIMIT="$FpsLimit"; MM3_DEBUG_TELEPORT='1' }
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
    $nextPress = 0
    while ($end.Elapsed.TotalSeconds -lt $Secs) {
        if ($end.Elapsed.TotalSeconds -ge $nextPress) {
            [IO.File]::AppendAllText($pad, "rt:2100`n"); $nextPress += 2
        }
        foreach ($c in cars) {
            if (-not $samples.ContainsKey($c.id)) { $samples[$c.id] = [Collections.ArrayList]::new() }
            [void]$samples[$c.id].Add($c)
        }
        Start-Sleep -Milliseconds 250
    }
} finally {
    if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit(15000) | Out-Null }
    Remove-Item -Recurse -Force (Join-Path $dir 'save') -ErrorAction SilentlyContinue   # 5 GB per run
}

$fail = $false
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
if (-not $playerRow -or $playerRow.forward -le $playerRow.backward) {
    Write-Host 'WARNING: player car did not come out forwards under throttle; forward vector sign is suspect'
}
Write-Host ("fps limit {0}: {1}" -f $FpsLimit, $(if ($fail) { 'FAIL' } else { 'PASS' }))
exit $(if ($fail) { 1 } else { 0 })
