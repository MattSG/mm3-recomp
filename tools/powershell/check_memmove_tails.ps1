param([string]$GeneratorDir = 'conformance_tmp/mm3_neg_flags_gen_20261001')
$ErrorActionPreference = 'Stop'
$GeneratorDir = (Resolve-Path $GeneratorDir).Path
$checkDir = Join-Path $GeneratorDir 'checks'
New-Item -ItemType Directory -Path $checkDir -Force | Out-Null
$checkPath = Join-Path $checkDir 'check_memmove_tails.c'
# Compile the actual generated tails against the actual runtime header.
@'
#include "recomp_types.h"
#include <assert.h>
#include <stdio.h>
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
RECOMP_TLS uint32_t g_ebx, g_esi, g_edi, g_ebp, g_seh_ebp;
void sub_00093B58_gen(void);
void sub_00093B60_gen(void);
void sub_00093B70_gen(void);
void sub_00093B84_gen(void);
int main(void) {
    unsigned char memory[2048];
    void (*tails[])(void) = {sub_00093B58_gen, sub_00093B60_gen,
                            sub_00093B70_gen, sub_00093B84_gen};
    unsigned remainder, i;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    for (remainder = 0; remainder < 4; ++remainder) {
        memset(memory, 0, sizeof(memory));
        g_ebp = g_seh_ebp = 0x100;
        g_esp = 0xF8;
        MEM32(0xF8) = 0xABC123;
        MEM32(0xFC) = 0xDEF456;
        MEM32(0x100) = 0x180;
        MEM32(0x104) = 0x123456;
        MEM32(0x108) = 0x400;
        g_esi = 0x300; g_edi = 0x400;
        MEM8(0x301) = 11; MEM8(0x302) = 22; MEM8(0x303) = 33;
        tails[remainder]();
        assert(g_esp == 0x108 && g_eax == 0x400);
        assert(g_esi == 0xABC123 && g_edi == 0xDEF456);
        for (i = 0; i < 4; ++i)
            assert(MEM8(0x400 + i) ==
                   (i && i >= 4 - remainder ? MEM8(0x300 + i) : 0));
    }
    puts("PASS: memmove remainder tails preserve stack and registers");
    return 0;
}
'@ | Set-Content -LiteralPath $checkPath -Encoding ascii
$tails = @('00093b58', '00093b60', '00093b70', '00093b84') | ForEach-Object {
    Join-Path $GeneratorDir "recomp_${_}_tail.c"
}
$exe = Join-Path $checkDir 'check_memmove_tails.exe'
$sharedInclude = (Resolve-Path (Join-Path $PSScriptRoot '..\..\tools\xboxrecomp\include')).Path
Push-Location $checkDir
try {
    & cl.exe /nologo /W3 /I $GeneratorDir /I $sharedInclude $checkPath @tails "/Fe:$exe"
    if ($LASTEXITCODE) { throw 'Memmove tail check compilation failed' }
    & $exe
    if ($LASTEXITCODE) { throw 'Memmove tail check failed' }
} finally { Pop-Location }
