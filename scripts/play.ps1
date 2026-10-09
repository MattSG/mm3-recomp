<#
.SYNOPSIS
Run the recompiled Midtown Madness 3 built by scripts\setup.ps1.

.DESCRIPTION
  pwsh scripts\play.ps1 [-Keyboard] [-Fullscreen] [-FpsLimit 120] [-Exe <path>]

Any RECOMP_*/MM3_* variable already set in your shell overrides these
defaults (see README "Options"). F11 or Alt+Enter toggles fullscreen.
#>
param(
    [switch]$Keyboard,
    [switch]$Fullscreen,
    [ValidateRange(1, 120)][int]$FpsLimit = 60,   # over 120 the race AI misdrives (README)
    [string]$Exe
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Exe) {
    $Exe = @('build\RelWithDebInfo\mm3_recomp.exe', 'build\Release\mm3_recomp.exe') |
        ForEach-Object { Join-Path $repo $_ } | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Exe -or -not (Test-Path $Exe)) { throw 'No build found: run scripts\setup.ps1 first.' }
if (-not (Test-Path (Join-Path $repo 'game_files\default.xbe'))) { throw 'game_files\ missing: run scripts\setup.ps1 -Iso <disc>.' }

$defaults = @{
    RECOMP_PB_EXEC = '1'; RECOMP_FB_WINDOW = '1'; RECOMP_USB = '1'
    RECOMP_APU_DSP_ACK = '0x80458810'; RECOMP_FPS_LIMIT = "$FpsLimit"
}
if ($Keyboard) { $defaults.RECOMP_KEYBOARD = '1' }
if ($Fullscreen) { $defaults.RECOMP_FULLSCREEN = '1' }
foreach ($k in $defaults.Keys) {
    if (-not [Environment]::GetEnvironmentVariable($k)) { [Environment]::SetEnvironmentVariable($k, $defaults[$k], 'Process') }
}
# The game reads game_files\ relative to the working directory.
Push-Location $repo
try { & $Exe } finally { Pop-Location }
