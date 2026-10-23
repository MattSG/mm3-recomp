# Historical APU device observations

Exact comment blocks from fork bb4f7e1, src/apu/apu_core.c, normalized to LF.
These are inherited title observations, not newly reproduced evidence.
Runtime address resolution, interrupt delivery, register startup and lock
scheduling stay in the fork. This migration changes no executable tokens.
Copyright/license notices remain in their original sources.

```c
/* Physical addresses, resolved the way every other bus master here does it
 * (dma_resolve in nv2a_pb_exec.c, bus_resolve in usb/ohci.c).
 *
 * DirectSound builds its voice, SGE and notifier structures in
 * MmAllocateContiguousMemory and hands the APU their physical addresses.
 * Physical P and the contiguous window's 0x80000000 + P are the same bytes on
 * hardware; here the window is separate storage. Reading every address as low
 * RAM meant the voice processor walked zeroes and wrote each "voice done"
 * notification into ordinary RAM, where DirectSound never looked -- so a
 * buffer never reported that it had stopped, and Burnout 3's frontend waits
 * on exactly that (IDirectSoundBuffer::GetStatus, polled forever). */
```

```c
/* HalGetInterruptVector(5) -- the APU's IRQ -- is what DirectSound connects
 * its service routine to (0x002F8E81 in Burnout 3). */
```

```c
/* Starting the APU has to start the frame thread.
         *
         * The thread idles on pause_requested, which init sets and only the
         * test tone ever cleared -- so a title that enabled the APU through
         * these registers got an APU that stayed asleep. Nothing then advanced
         * the front end, and a title waiting on a notify completion (the
         * FEMEMDATA magic write, which is how completion reaches guest memory)
         * waited forever. Wreckless hangs exactly there during DirectSound
         * init, and because it initialises its whole engine behind a
         * successful DirectSound create, that hang is not confined to audio.
         *
         * Resume whenever the write is not switching the block off; the thread
         * re-checks FECTL itself and idles again if it is halted or trapped. */
```

```c
/* Let the guest in once per frame.
         *
         * The thread holds d->lock for its whole loop and only drops it inside
         * throttle()'s wait. Once voices really play, processing can run
         * behind real time, throttle never waits, and the lock is never
         * released -- while every VOICE_ON/OFF/RELEASE the title writes needs
         * it (voice_lock). A critical section is not fair, so the title's
         * thread starved there indefinitely: Burnout 3 froze on its vehicle
         * select, blocked in voice_lock at raised IRQL, which in turn held
         * off every USB interrupt. */
```
