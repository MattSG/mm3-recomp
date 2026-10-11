<#
.SYNOPSIS
Run the recompiled Midtown Madness 3 built by scripts\setup.ps1.

.DESCRIPTION
  pwsh scripts\play.ps1 [-Keyboard] [-Fullscreen] [-Profile Balanced|Quality|LowPower] [-FpsLimit 60] [-Fov 100] [-Exe <path>]

Any RECOMP_*/MM3_* variable already set in your shell overrides these
defaults (see README "Options"). F11 or Alt+Enter toggles fullscreen.
#>
param(
    [switch]$Keyboard,
    [switch]$Fullscreen,
    [ValidateSet('Balanced', 'Quality', 'LowPower')][string]$Profile = 'Balanced',
    [ValidateRange(1, 60)][int]$FpsLimit,
    [ValidateRange(60, 110)][double]$Fov,
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
    RECOMP_APU_DSP_ACK = '0x80458810'; MM3_PERFORMANCE_PROFILE = $Profile
    RECOMP_KEYBOARD_LAYOUT = 'mm3'
    RECOMP_ASPECT = '16:10'
}
if ($PSBoundParameters.ContainsKey('FpsLimit')) { $defaults.RECOMP_FPS_LIMIT = "$FpsLimit" }
if ($PSBoundParameters.ContainsKey('Fov')) {
    $defaults.MM3_FOV_DEGREES = $Fov.ToString([Globalization.CultureInfo]::InvariantCulture)
}
if ($Keyboard) { $defaults.RECOMP_KEYBOARD = '1' }
if ($Fullscreen) { $defaults.RECOMP_FULLSCREEN = '1' }
# The game reads game_files\ relative to the working directory.
$previous = @{}
Push-Location $repo
try {
    foreach ($k in $defaults.Keys) {
        $value = [Environment]::GetEnvironmentVariable($k, 'Process')
        if (-not $value) {
            $previous[$k] = $value
            [Environment]::SetEnvironmentVariable($k, $defaults[$k], 'Process')
        }
    }
    & $Exe
} finally {
    foreach ($k in $previous.Keys) {
        if ($null -eq $previous[$k]) {
            Remove-Item -LiteralPath "Env:\$k" -ErrorAction SilentlyContinue
        } else {
            [Environment]::SetEnvironmentVariable($k, $previous[$k], 'Process')
        }
    }
    Pop-Location
}
