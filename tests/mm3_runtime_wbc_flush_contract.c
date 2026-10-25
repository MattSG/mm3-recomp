/* xbox_Nv2aWbcFlush: the flush word is set, the call waits for the executor
 * to clear it, and the register block and flush value are reported. Windows
 * only, like the threads it uses. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "nv2a_wbc_flush.h"
#include "nv2a_regs.h"

static unsigned char ram[0x300000];
static ptrdiff_t g_mem_offset;
ptrdiff_t xbox_GetMemoryOffset(void) { return g_mem_offset; }
#define GUEST32(a) (*(volatile uint32_t *)(ram + (a)))

/* Device object at 0x2000 holds its register block pointer at +0x17C8. */
#define DEVICE 0x2000u
#define REGS 0x10000u
#define FLUSH (REGS + 0x00100000u + NV_PFB_WBC)

static volatile LONG acknowledged;

static DWORD WINAPI acknowledge(void *unused)
{
    ULONGLONG start = GetTickCount64();
    (void)unused;
    while (!(GUEST32(FLUSH) & NV_PFB_WBC_FLUSH) && GetTickCount64() - start < 500) Sleep(0);
    if (GUEST32(FLUSH) & NV_PFB_WBC_FLUSH) {
        Sleep(20);
        GUEST32(FLUSH) &= ~NV_PFB_WBC_FLUSH;
        InterlockedExchange(&acknowledged, 1);
    }
    return 0;
}

int main(void)
{
    uint32_t object = 0, value = 0;
    HANDLE thread;
    g_mem_offset = (ptrdiff_t)ram;
    GUEST32(DEVICE) = 0x4000;
    GUEST32(0x4000 + 0x17C8) = REGS;
    GUEST32(FLUSH) = 0x1234;
    acknowledged = 0;
    thread = CreateThread(NULL, 0, acknowledge, NULL, 0, NULL);
    assert(thread);
    xbox_Nv2aWbcFlush(DEVICE, &object, &value);
    assert(WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    /* The call returned only after the executor cleared the bit. */
    assert(acknowledged == 1);
    assert(!(GUEST32(FLUSH) & NV_PFB_WBC_FLUSH));
    assert(object == REGS);
    assert(value == ((0x1234u | NV_PFB_WBC_FLUSH)));
    printf("PASS: flush bit set, call waits for acknowledgement, object and value reported\n");
    return 0;
}
