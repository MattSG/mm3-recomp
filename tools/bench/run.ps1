<#
Repeatable menu + race benchmark (uncapped unless -FpsLimit).

  pwsh tools\bench\run.ps1 -Exe <mm3_recomp.exe> -Save <save template dir> [-Tag name] [-FpsLimit 0] [-Secs 20]

The save template is a copy of an MM3 save folder (HDD partition images with a
profile); each run gets a fresh copy under conformance_tmp\bench\<tag>, so the
real saves are never touched. Settings are pinned: 16:9, 1880 internal lines
(RECOMP_RENDER_SCALE 3.9167), default MSAA/AF/draw distance. Menu: 10 s on the
main menu. Race: the menu inputs below lead to a race; the car is teleported to
a fixed spot (MM3_DEBUG_TELEPORT) so every run renders the same scene.

Output per window: frame-time percentiles, 1%/0.1% lows, jitter and hitches
(from RECOMP_FRAME_CSV via frames.py), executor ms/frame (RECOMP_D3D_PROFILE)
and average GPU utilisation (nvidia-smi; GPU busy per frame ~= util x frame
time). Menu input is time-based: if the race is not reached, "a" is pressed
again until a car is live.
#>
param([Parameter(Mandatory)][string]$Exe, [Parameter(Mandatory)][string]$Save,
      [string]$Tag = 'bench', [int]$FpsLimit = 0, [int]$Secs = 20,
      [double]$X = 97.66, [double]$Z = -1028.3)   # Washington; the save's city decides
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dir = Join-Path $repo "conformance_tmp\bench\$Tag"
Get-Process mm3_recomp -ErrorAction SilentlyContinue | Stop-Process -Force
if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
New-Item -ItemType Directory -Force $dir | Out-Null
Copy-Item -Recurse $Save (Join-Path $dir 'save')
$pad = Join-Path $dir 'pad.txt'; [IO.File]::WriteAllText($pad, '')
$csv = Join-Path $dir 'frames.csv'; $log = Join-Path $dir 'stderr.log'
$envs = @{ RECOMP_PB_EXEC='1'; RECOMP_FB_WINDOW='1'; RECOMP_USB='1'; RECOMP_PAD_LIVE=$pad
           RECOMP_APU_DSP_ACK='0x80458810'; MM3_SAVE_DIR=(Join-Path $dir 'save')
           RECOMP_ASPECT='16:9'; RECOMP_RENDER_SCALE='3.9167'; RECOMP_FPS_LIMIT="$FpsLimit"
           RECOMP_D3D_PROFILE='1'; MM3_DEBUG_TELEPORT='1'; RECOMP_FRAME_CSV=$csv
           RECOMP_PRESENT_CAPTURE=(Join-Path $dir 'cap'); RECOMP_CAPTURE_SURFACE='1' }
foreach ($k in $envs.Keys) { Set-Item "Env:$k" $envs[$k] }
$p = Start-Process -FilePath $Exe -WorkingDirectory $repo -PassThru `
     -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError $log
foreach ($k in $envs.Keys) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }

function press([string]$b, [double]$after) {
    [IO.File]::AppendAllText($pad, "$($b):150`n"); Start-Sleep -Milliseconds 100; Start-Sleep $after }
function qms { [Diagnostics.Stopwatch]::GetTimestamp() * 1000.0 / [Diagnostics.Stopwatch]::Frequency }
$windows = @()
function window([string]$name, [int]$s) {
    $n0 = @(Select-String $log -Pattern 'D3D11_PROF').Count
    $smi = Join-Path $dir "smi_$name.txt"
    $sp = Start-Process nvidia-smi -ArgumentList @('--query-gpu=utilization.gpu', '--format=csv,noheader,nounits', '-lms', '250') `
          -RedirectStandardOutput $smi -WindowStyle Hidden -PassThru
    $a = qms; Start-Sleep $s; $b = qms
    Stop-Process -Id $sp.Id -ErrorAction SilentlyContinue
    $ex = @(Select-String $log -Pattern 'D3D11_PROF' | Select-Object -Skip $n0 |
            ForEach-Object { [double]($_.Line -replace '.*exec\(all\) ([0-9.]+) ms.*', '$1') })
    $u = @(Get-Content $smi -ErrorAction SilentlyContinue | Where-Object { $_ -match '^\s*\d+' } | ForEach-Object { [double]$_ })
    $script:windows += , @($name, $a, $b, ('executor {0:N2} ms | GPU util {1:N0}%' -f ($ex | Measure-Object -Average).Average, ($u | Measure-Object -Average).Average))
}

# Intro movies: skip until the menu presents frames.
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt 120) {
    Start-Sleep -Milliseconds 700
    $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
    if ($t -match 'MOVIE_END\] name=intro') { break }
    if ($t -match 'MOVIE_FRAME') { press 'start' 0 }
}
Start-Sleep 5
press 'start' 4; press 'a' 6                       # title -> profile -> main menu
window 'menu' 10
foreach ($s in @(@('down', 1), @('a', 4), @('a', 4), @('a', 5), @('a', 8), @('a', 10), @('a', 9), @('start', 6))) { press $s[0] $s[1] }
$tp = Join-Path $repo 'tools\powershell\Send-MM3Teleport.ps1'
for ($t = 0; $t -lt 5; $t++) {
    $r = $null; try { $r = & $tp -ProcessId $p.Id } catch {}
    if ($r -and $r.available) { break }
    press 'a' 9
}
try { & $tp -ProcessId $p.Id -X $X -Z $Z -Heading 0 | Out-Null }
catch { Write-Warning "teleport failed ($_): race measured at the spawn point $(& $tp -ProcessId $p.Id | ConvertTo-Json -Compress), not comparable across runs" }
Start-Sleep 4
window 'race' $Secs
Start-Sleep 2
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
$p.WaitForExit(15000) | Out-Null
Remove-Item -Recurse -Force (Join-Path $dir 'save') -ErrorAction SilentlyContinue   # 5 GB per run
foreach ($w in $windows) {
    "{0,-5}: {1} | {2}" -f $w[0], (python (Join-Path $repo 'tools/xboxrecomp/tools/bench/frames.py') $csv $w[1] $w[2]), $w[3]
}
