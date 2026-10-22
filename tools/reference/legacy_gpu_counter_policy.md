# Historical GPU counter compatibility policy

Source: XboxRecomp `f507c486bcc8e7fdd11ea10c47f293b3ad3543c1`, `src/kernel/xbox_memory_layout.c`.

The original block below contains captured dashboard/Wreckless/Half-Life 2
observations, unconditional submitted-to-completed copies and a clock-driven
frame-counter fallback. Neither policy has an active MM3 registration. They
are retained as port investigation history, not generic GPU completion. The
fork retains registered counters driven by executed flips. Ports depending on
synthetic progress must own that policy rather than infer completed GPU work.

```c
/*
 * Two counters inside the device, one of which the GPU owns.
 *
 * D3D's swap throttle is a pair: the title bumps "frames submitted" itself and
 * waits for "frames completed", which on hardware only the GPU moves. The Xbox
 * dashboard's is exactly that, at guest 0x000AF121 --
 *
 *     eax = [esi+0x2518]        ; completed
 *     ecx = [esi+0x2B60]        ; submitted
 *     ecx = ecx - eax
 *     if (ecx < 2) proceed      ; else spin on a 400-iteration delay loop
 *
 * -- and with nothing moving completed it spins there forever once two frames
 * are outstanding. That delay loop was 99.8 million of the dashboard's calls,
 * against 35 thousand for the next function down.
 *
 * This differs from xbox_Nv2aFrameCounter, which advances a counter on a 60 Hz
 * clock, in the way that matters for this pair: a free-running counter can
 * pass submitted, and then submitted - completed underflows to about four
 * billion, which is >= 2, and the spin never ends again. Mirroring cannot do
 * that, because completed is only ever whatever submitted already is.
 *
 * It is also simply true here. The pushbuffer is executed at submit, so by the
 * time the title asks whether the frame is finished, it is.
 */
#define XBOX_MAX_COUNTER_MIRRORS 4

static struct {
    uint32_t device_ptr_va;
    uint32_t src_off, dst_off;
} g_counter_mirrors[XBOX_MAX_COUNTER_MIRRORS];
static int g_counter_mirror_count = 0;

int xbox_Nv2aMirrorCounter(uint32_t device_ptr_va,
                           uint32_t src_off, uint32_t dst_off)
{
    if (g_counter_mirror_count >= XBOX_MAX_COUNTER_MIRRORS)
        return -1;
    g_counter_mirrors[g_counter_mirror_count].device_ptr_va = device_ptr_va;
    g_counter_mirrors[g_counter_mirror_count].src_off = src_off;
    g_counter_mirrors[g_counter_mirror_count].dst_off = dst_off;
    g_counter_mirror_count++;
    fprintf(stderr, "  NV2A counter mirror: device at 0x%08X,"
            " +0x%X -> +0x%X\n", device_ptr_va, src_off, dst_off);
    return 0;
}

static void counter_mirrors_tick(void)
{
    for (int i = 0; i < g_counter_mirror_count; i++) {
        uint32_t dev;

        if (!fence_readable(g_counter_mirrors[i].device_ptr_va, 4))
            continue;
        dev = *(volatile uint32_t *)((uintptr_t)g_counter_mirrors[i].device_ptr_va
                                     + g_memory_offset);
        if (!fence_readable(dev + g_counter_mirrors[i].src_off, 4)
                || !fence_readable(dev + g_counter_mirrors[i].dst_off, 4))
            continue;
        {
            volatile uint32_t *dst =
                (volatile uint32_t *)((uintptr_t)(dev + g_counter_mirrors[i].dst_off)
                                      + g_memory_offset);
            uint32_t src =
                *(volatile uint32_t *)((uintptr_t)(dev + g_counter_mirrors[i].src_off)
                                       + g_memory_offset);
            if (*dst != src)
                *dst = src;
        }
    }
}

/*
 * Frame counters the title polls to pace itself.
 *
 * D3D keeps a swap count inside the device and bumps it once per presented
 * frame; a title that wants to wait a frame reads it and spins until it moves.
 * Wreckless does exactly that at guest 0x000DC5E0 -- "loop while the counter
 * has advanced by less than 2" -- so a counter that never moves is not a
 * dropped frame, it is a hang with a full asset load behind it.
 *
 * Nothing here presents, so nothing would ever move it. Advancing it on a
 * clock is what makes the wait terminate, and 60 Hz is the rate the title
 * expects the display to run at. Followed through the device pointer for the
 * same reason the fence is: the device is allocated at runtime.
 */
#define XBOX_MAX_FRAME_COUNTERS 4
#define XBOX_FRAME_PERIOD_MS    16      /* ~60 Hz */

static struct {
    uint32_t device_ptr_va;
    uint32_t counter_off;
} g_frame_counters[XBOX_MAX_FRAME_COUNTERS];
static int   g_frame_counter_count = 0;
static DWORD g_frame_counter_last_ms = 0;

int xbox_Nv2aFrameCounter(uint32_t device_ptr_va, uint32_t counter_off)
{
    if (g_frame_counter_count >= XBOX_MAX_FRAME_COUNTERS)
        return -1;
    g_frame_counters[g_frame_counter_count].device_ptr_va = device_ptr_va;
    g_frame_counters[g_frame_counter_count].counter_off   = counter_off;
    g_frame_counter_count++;
    fprintf(stderr, "  Frame counter: device at 0x%08X, count +0x%X @ %d Hz\n",
            device_ptr_va, counter_off, 1000 / XBOX_FRAME_PERIOD_MS);
    return 0;
}

/* A real swap happened: advance every registered counter, and remember when.
 *
 * The timer below exists for a title nothing presents for. Once the
 * pushbuffer executor is actually running flips, the timer is the wrong
 * clock and an actively harmful one: Half-Life 2's loader paces its intro on
 * this count, so a 62 Hz timer against an executor managing a fraction of a
 * frame per second ran the video forward in virtual time far faster than it
 * could be drawn. Only every few hundredth frame was ever presented, each one
 * sampled part way through its own decode -- which looks exactly like a
 * stalling, blocky video rather than a clock running away.
 */
static DWORD g_frame_counter_flip_ms;

void xbox_Nv2aFrameCounterFlip(void)
{
    int i;

    g_frame_counter_flip_ms = GetTickCount();
    if (!g_frame_counter_flip_ms)
        g_frame_counter_flip_ms = 1;          /* 0 means "never" */
    for (i = 0; i < g_frame_counter_count; i++) {
        uint32_t dev;

        if (!fence_readable(g_frame_counters[i].device_ptr_va, 4))
            continue;
        dev = *(volatile uint32_t *)((uintptr_t)g_frame_counters[i].device_ptr_va
                                     + g_memory_offset);
        if (!fence_readable(dev + g_frame_counters[i].counter_off, 4))
            continue;
        *(volatile uint32_t *)((uintptr_t)(dev + g_frame_counters[i].counter_off)
                               + g_memory_offset) += 1;
    }
}

static void frame_counters_tick(void)
{
    DWORD now;
    int i;

    if (!g_frame_counter_count)
        return;
    now = GetTickCount();
    if (g_frame_counter_last_ms
            && (now - g_frame_counter_last_ms) < XBOX_FRAME_PERIOD_MS)
        return;
    /* Something is presenting: let it drive the count instead. Two seconds,
     * because the executor's flips are not evenly spaced and a title that
     * genuinely stops presenting still has to be got moving again. */
    if (g_frame_counter_flip_ms && (now - g_frame_counter_flip_ms) < 2000) {
        g_frame_counter_last_ms = now;
        return;
    }
    g_frame_counter_last_ms = now;

    for (i = 0; i < g_frame_counter_count; i++) {
        uint32_t dev;

        if (!fence_readable(g_frame_counters[i].device_ptr_va, 4))
            continue;
        dev = *(volatile uint32_t *)((uintptr_t)g_frame_counters[i].device_ptr_va
                                     + g_memory_offset);
        if (!fence_readable(dev + g_frame_counters[i].counter_off, 4))
            continue;
        *(volatile uint32_t *)((uintptr_t)(dev + g_frame_counters[i].counter_off)
                               + g_memory_offset) += 1;
    }
}
```

## Original API commentary and declarations

```c
/**
 * Release the reserved Xbox memory layout.
 */
/**
 * Mirror a GPU completion fence the title spins on.
 *
 * The NV2A tables in xbox_memory_layout.c acknowledge handshakes that live at
 * fixed aperture offsets. Some titles instead wait on a semaphore the GPU
 * writes into contiguous memory: D3D seeds it, submits work, then spins until
 * it reaches the submitted count. Wreckless does this at guest 0x000FE920,
 * waiting on the 96-byte MmAllocateContiguousMemoryEx block its device struct
 * points at.
 *
 * Nothing executes the push buffer -- the D3D11 layer draws -- so everything
 * submitted is complete, and advancing the fence is the same honest
 * acknowledgement the register tables make.
 *
 * The fence has no fixed address; it is reached through the title's device
 * struct, so it is registered as the chain of indirections to follow:
 *
 *     device = MEM32(device_ptr_va)
 *     fence  = MEM32(device + get_ptr_off)
 *     MEM32(fence) = MEM32(device + put_off)
 *
 * Every step is bounds-checked each poll, so registering a chain that is not
 * yet initialised (or never becomes valid) is harmless.
 *
 * Returns 0 on success, -1 if the table is full.
 */
/* Advance a frame/swap counter inside the D3D device at ~60 Hz.
 *
 * A title that waits a frame reads the device's swap count and spins until it
 * moves. Nothing here presents, so without this the count never changes and
 * the wait never ends. Followed through the device pointer, like the fence,
 * because the device is allocated at runtime.
 *
 * Returns 0 on success, -1 if the table is full. */
/* Mirror one field of the device struct onto another, for the counter pair a
 * swap throttle waits on: the title bumps "frames submitted" itself and waits
 * for "frames completed", which only the GPU moves on hardware. Unlike
 * xbox_Nv2aFrameCounter this cannot run ahead of the title, so the unsigned
 * submitted - completed it gets compared against cannot underflow. */
int xbox_Nv2aMirrorCounter(uint32_t device_ptr_va,
                           uint32_t src_off, uint32_t dst_off);

int xbox_Nv2aFrameCounter(uint32_t device_ptr_va, uint32_t counter_off);
/* Advance those counters now, because a swap really completed. Called by the
 * pushbuffer executor on FLIP_STALL; while these arrive the 60 Hz fallback
 * stands down, so the count follows what was actually drawn. */
void xbox_Nv2aFrameCounterFlip(void);
```

## Original snapshot-derived fence publication

```c
/* What the fence may claim: the fences submitted when a push-buffer segment
 * starts executing, published once it has executed (and its flip has been
 * read back), with DMA_GET. Claiming them at the top of the tick, before the
 * segment ran, let a title that runs ahead of the executor recycle buffers
 * its queued draws still read. Fences the title has inserted but not yet
 * kicked off still count as done, as they always have here. */
static uint32_t s_fence_snap[XBOX_MAX_FENCE_MIRRORS];
static int s_fence_snap_ok[XBOX_MAX_FENCE_MIRRORS];

static void fence_mirrors_tick(int publish)
{
    int changed = 0;
    for (int i = 0; i < g_fence_mirror_count; i++) {
        uint32_t dev, get_ptr;

        if (g_fence_mirrors[i].command_mode) continue;

        if (!fence_readable(g_fence_mirrors[i].device_ptr_va, 4))
            continue;
        dev = *(volatile uint32_t *)((uintptr_t)g_fence_mirrors[i].device_ptr_va
                                     + g_memory_offset);
        if (!fence_readable(dev + g_fence_mirrors[i].get_ptr_off, 4)
                || !fence_readable(dev + g_fence_mirrors[i].put_off, 4)) {
            /* dev is 0 until the title creates the device; that is not a stall. */
            if (dev)
                fence_skip_note(i, FENCE_SKIP_TICK_CHAIN, "GET/PUT slot",
                                dev + g_fence_mirrors[i].get_ptr_off);
            continue;
        }
        get_ptr = *(volatile uint32_t *)((uintptr_t)(dev + g_fence_mirrors[i].get_ptr_off)
                                         + g_memory_offset);
        if (!fence_readable(get_ptr, 4)) {
            fence_skip_note(i, FENCE_SKIP_TICK_CHAIN, "fence", get_ptr);
            continue;
        }
        {
            volatile uint32_t *fence =
                (volatile uint32_t *)((uintptr_t)get_ptr + g_memory_offset);
            uint32_t put =
                *(volatile uint32_t *)((uintptr_t)(dev + g_fence_mirrors[i].put_off)
                                       + g_memory_offset);
            if (!publish) {
                s_fence_snap[i] = put;
                s_fence_snap_ok[i] = 1;
            } else if (s_fence_snap_ok[i] && *fence != s_fence_snap[i]) {
                *fence = s_fence_snap[i];
                changed = 1;
            }
        }
    }
    if (changed) xbox_nv2a_wake_publish(&g_nv2a_wake);
}
```
