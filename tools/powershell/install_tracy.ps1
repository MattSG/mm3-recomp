[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$version = '0.14.1'
$sha256 = 'f7499d74914aa3ba94a2c1ce72f36477d7b61d9d0f7c9790e05274c258c97fb5'
$url = "https://github.com/wolfpld/tracy/releases/download/v$version/windows-$version.zip"
$cache = Join-Path $repoRoot 'tools\cache\tracy'
$archive = Join-Path $cache "windows-$version.zip"
$install = Join-Path $repoRoot "tools\bin\tracy-$version"
$profiler = Join-Path $install 'tracy-profiler.exe'

if (Test-Path -LiteralPath $profiler) {
    Write-Host "Tracy $version already installed: $profiler"
    return
}

New-Item -ItemType Directory -Path $cache -Force | Out-Null
Invoke-WebRequest -Uri $url -OutFile $archive
$actualHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualHash -ne $sha256) {
    Remove-Item -LiteralPath $archive -Force
    throw "Tracy archive SHA-256 mismatch: $actualHash"
}

New-Item -ItemType Directory -Path $install -Force | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $install -Force
if (-not (Test-Path -LiteralPath $profiler)) {
    throw "Tracy archive did not contain tracy-profiler.exe: $install"
}
Write-Host "Installed Tracy ${version}: $profiler"

