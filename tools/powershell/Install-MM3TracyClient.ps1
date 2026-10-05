[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$version = '0.14.1'
$install = Join-Path $repoRoot "tools\bin\tracy-$version"
if (Test-Path -LiteralPath (Join-Path $install 'public\TracyClient.cpp')) { return }
$cache = Join-Path $repoRoot 'tools\cache\tracy'
New-Item -ItemType Directory -Force -Path $cache | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot 'tools\bin') | Out-Null
$archive = Join-Path $cache "tracy-source-$version.tar.gz"
Invoke-WebRequest -Uri "https://codeload.github.com/wolfpld/tracy/tar.gz/refs/tags/v$version" -OutFile $archive
$expected = 'bf4af567e9c7524d07f3caa745fad02fb33bd5694f11910750382d1efbb251c1'
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw 'Tracy client archive hash mismatch'
}
& tar -xf $archive -C (Join-Path $repoRoot 'tools\bin')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $install 'public\TracyClient.cpp'))) {
    throw 'Tracy client extraction failed'
}
