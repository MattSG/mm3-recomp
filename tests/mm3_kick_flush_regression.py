"""Exercise the production XDK cache-flush handshake with an acknowledging thread."""
from pathlib import Path
import subprocess
import tempfile
import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    source = (ROOT / "src/recomp_manual.c").read_text(encoding="utf-8")
    start = source.index("static void mm3_kick_wait_ack(")
    kick = source.index("void sub_00344AB0(", start)
    end = source.index("\n}\n", kick) + 3
    fixture = r'''
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <immintrin.h>
static unsigned char ram[0x360000];
#define MEM32(a) (*(volatile uint32_t *)(ram + (a)))
static uint32_t g_eax, g_ecx, g_esp;
static unsigned watch_calls;
void recomp_sampler_watch_current_thread(void) { ++watch_calls; }
''' + source[start:end] + r'''
static volatile LONG observed;
static DWORD WINAPI acknowledge(void *unused)
{
    ULONGLONG start = GetTickCount64();
    (void)unused;
    while (!(MEM32(0x110410) & 0x10000u) && GetTickCount64()-start < 500) Sleep(0);
    if (MEM32(0x110410) & 0x10000u) {
        InterlockedExchange(&observed, 1);
        MEM32(0x110410) &= ~0x10000u;
    }
    return 0;
}
int main(void)
{
    const uint32_t positions[][2] = {{0x20000,0x20000},{0x30000,0x20000},
                                     {0x90000,0x10000},{0x10000,0x90000}};
    unsigned i;
    MEM32(0x351F48)=0x2000; MEM32(0x2000+0x17C8)=0x10000;
    MEM32(0x2000+0x17C0)=0x3000;
    for (i=0; i<4; ++i) {
        HANDLE thread;
        MEM32(0x2000)=positions[i][0]; MEM32(0x3044)=positions[i][1];
        MEM32(0x110410)=0x15; observed=0; g_esp=0x400;
        thread=CreateThread(NULL,0,acknowledge,NULL,0,NULL); assert(thread);
        sub_00344AB0();
        assert(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0); CloseHandle(thread);
        assert(observed==1); assert(MEM32(0x110410)==0x15);
        assert(g_esp==0x404 && g_eax==0x10000 && g_ecx==0x10015);
    }
    assert(watch_calls==1);
    puts("PASS: every XDK flush waits for acknowledgement, including zero/small lag and wrap");
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="mm3-kick-flush-") as folder:
        folder = Path(folder)
        cfile = folder / "flush.c"
        cfile.write_text(fixture, encoding="utf-8")
        exe = folder / "flush.exe"
        subprocess.run(cc.compile_command([cfile], exe, std="c11", optimize="none",
                        warnings="high", obj=folder / "flush.obj"), check=True, cwd=folder)
        subprocess.run([str(exe)], check=True, timeout=10)


if __name__ == "__main__":
    main()
