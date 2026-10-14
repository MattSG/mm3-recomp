<#
.SYNOPSIS
Activate this checkout's local Python, PowerShell and C++ tools for this session.

.DESCRIPTION
  . .\scripts\enter-env.ps1

Only the current process environment changes; nothing is added to the system PATH.
#>
$localRepo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$localMsvcSetup = Join-Path $localRepo 'tools\bin\msvc\setup_x64.bat'
if (Test-Path -LiteralPath $localMsvcSetup) {
    $localMsvcVariables = & $env:ComSpec /d /s /c ('call "{0}" >nul && set' -f $localMsvcSetup)
    if ($LASTEXITCODE -ne 0) { throw 'Could not activate the local MSVC toolchain.' }
    foreach ($localVariable in $localMsvcVariables) {
        if ($localVariable -match '^([^=]+)=(.*)$') {
            Set-Item -LiteralPath "Env:\$($Matches[1])" -Value $Matches[2]
        }
    }
}
foreach ($localToolDir in @('tools\bin\powershell', 'tools\venv\Scripts')) {
    $localToolPath = Join-Path $localRepo $localToolDir
    if ((Test-Path -LiteralPath $localToolPath) -and ($env:Path -split ';') -notcontains $localToolPath) {
        $env:Path = "$localToolPath;$env:Path"
    }
}
$env:Path = (($env:Path -split ';') | Select-Object -Unique) -join ';'
$localVenv = Join-Path $localRepo 'tools\venv'
if (Test-Path -LiteralPath (Join-Path $localVenv 'Scripts\python.exe')) {
    $env:VIRTUAL_ENV = $localVenv
    Remove-Item Env:\PYTHONHOME -ErrorAction SilentlyContinue
}
$env:PYTHONUTF8 = '1'
