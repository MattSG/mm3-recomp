[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [int]$ProcessId,
    [Parameter(Mandatory)]
    [ValidateSet('Start', 'Back', 'A', 'B', 'X', 'Y', 'Up', 'Down', 'Left', 'Right')]
    [string]$Button,
    [ValidateRange(30, 2000)]
    [int]$HoldMilliseconds = 120
)

$ErrorActionPreference = 'Stop'
# Existing keyboard bridge mapping: Enter=START, Z=A, X=B, A=X, S=Y.
$virtualKey = @{
    Start = 0x0D; Back = 0x08
    A = 0x5A; B = 0x58; X = 0x41; Y = 0x53
    Up = 0x26; Down = 0x28; Left = 0x25; Right = 0x27
}[$Button]

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class MM3FramebufferInput {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
}
'@

$process = Get-Process -Id $ProcessId
$window = $process.MainWindowHandle
if ($window -eq [IntPtr]::Zero) { throw "Process $ProcessId has no framebuffer window" }

if (-not [MM3FramebufferInput]::PostMessage($window, 0x0100, [IntPtr]$virtualKey, [IntPtr]::Zero)) {
    throw "Could not post $Button key-down to process $ProcessId"
}
Start-Sleep -Milliseconds $HoldMilliseconds
if (-not [MM3FramebufferInput]::PostMessage($window, 0x0101, [IntPtr]$virtualKey, [IntPtr]::Zero)) {
    throw "Could not post $Button key-up to process $ProcessId"
}
Write-Output "Sent $Button to PID $ProcessId (VK 0x$('{0:X2}' -f $virtualKey))"
