param([string]$SourceFile)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dir = Join-Path $repo 'conformance_tmp/pvideo_register_check'
New-Item -ItemType Directory -Path $dir -Force | Out-Null
if (!$SourceFile) { $SourceFile = "$repo/tools/xboxrecomp/src/nv2a/nv2a_core.c" }
$source = Get-Content -LiteralPath $SourceFile -Raw
$prefix = @"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint64_t hwaddr;
typedef struct { struct { uint32_t regs[0x1000]; } pvideo; } NV2AState;
#define NV_PVIDEO_STOP 0x704u
#define NV_PVIDEO_BUFFER 0x700u
#define nv2a_reg_log_read(...) ((void)0)
#define nv2a_reg_log_write(...) ((void)0)
"@
$bodies = foreach ($name in @('pvideo_read','pvideo_write')) {
    $body = [regex]::Match($source, "(?ms)^(?:uint64_t|void) $name\(.*?^\}")
    if (-not $body.Success) { throw "Missing $name" }
    $body.Value
}
$checks = @"
int main(void) {
 NV2AState state = {0};
 pvideo_write(&state, NV_PVIDEO_BUFFER, 0x11, 4);
 pvideo_write(&state, NV_PVIDEO_STOP, 0, 4);
 assert(pvideo_read(&state, NV_PVIDEO_BUFFER, 4) == 0x11);
 assert(pvideo_read(&state, NV_PVIDEO_STOP, 4) == 0);
 pvideo_write(&state, 0x958, 0x10500, 4);
 assert(pvideo_read(&state, 0x958, 4) == 0x10500);
 pvideo_write(&state, NV_PVIDEO_STOP, 1, 4);
 assert(pvideo_read(&state, NV_PVIDEO_BUFFER, 4) == 0);
 assert(pvideo_read(&state, NV_PVIDEO_STOP, 4) == 0);
 pvideo_write(&state, NV_PVIDEO_BUFFER, 1, 4);
 assert(pvideo_read(&state, NV_PVIDEO_BUFFER, 4) == 1);
 puts("PASS: PVIDEO stop command clears banks, reads zero, and permits restart");
 return 0;
}
"@
($prefix + "`n" + ($bodies -join "`n") + "`n" + $checks) | Set-Content -LiteralPath "$dir/check.c" -Encoding ascii
& cl.exe /nologo /W3 "$dir/check.c" "/Fo:$dir/check.obj" "/Fe:$dir/check.exe"
if ($LASTEXITCODE) { throw 'PVIDEO check compilation failed' }
& "$dir/check.exe"
if ($LASTEXITCODE) { throw 'PVIDEO check failed' }

