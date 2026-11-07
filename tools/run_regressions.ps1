# Runs every tests/*_regression.py (and test_*.py) inside the VS dev environment
# and prints a pass/fail summary. Usage: pwsh tools/run_regressions.ps1 [-Filter pattern]
param([string]$Filter = '', [int]$TimeoutSec = 300)
$root = Split-Path $PSScriptRoot -Parent
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$bat = Join-Path $env:TEMP 'mm3_vc.bat'
Set-Content $bat "@call `"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul`r`n@set"
& "$env:ComSpec" /c $bat | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] }
}
$py = if (Test-Path "$root\.venv-tests\Scripts\python.exe") { "$root\.venv-tests\Scripts\python.exe" } else { 'python' }
$env:PYTHONPATH = $root
$fail = @(); $pass = 0
Get-ChildItem "$root\tests" -File | Where-Object { $_.Name -like "*$Filter*" -and $_.Name -match '(_regression|^test_.*)\.py$' } | ForEach-Object {
    $log = Join-Path $env:TEMP "reg_$($_.BaseName).log"
    $unit = $_.Name -like 'test_*'
    $args1 = if ($unit) { @('-m', 'unittest', $_.BaseName) } else { @($_.FullName) }
    $wd = if ($unit) { Join-Path $root 'tests' } else { $root }
    $p = Start-Process $py -ArgumentList $args1 -WorkingDirectory $wd -NoNewWindow -PassThru -RedirectStandardOutput $log -RedirectStandardError "$log.err"
    if (-not $p.WaitForExit($TimeoutSec * 1000)) { $p.Kill(); $fail += "$($_.Name) (timeout)"; return }
    if ($p.ExitCode -eq 0) { $pass++ } else { $fail += $_.Name; Write-Host "FAIL $($_.Name)  -> $log(.err)" }
}
Write-Host "passed=$pass failed=$($fail.Count)"
exit $fail.Count
