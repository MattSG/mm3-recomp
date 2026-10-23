# Historical RTL critical-section investigation

Captured title failure observations and one-run address relationships belong
here rather than in generic runtime source. They are history, not guarantees
about the current runtime or all guest programs. The shared-lock diagnostic can
still deadlock if a holder waits for another thread requiring that lock.

Removed source narratives:

```text
 * The failure that produced is worth describing, because it looks like
 * anything but a lock. Half-Life 2's level load faulted intermittently -- four
 * runs, three different functions, one clean -- and any attempt to observe it
 * made it disappear, including two plain stores to globals. Registers held
 * fragments of strings where pointers belonged, which is what a half-updated
 * structure looks like from the other thread.
 *
 * Deadlock-free by construction -- there is only one lock, so there is no order
 * to invert -- and deliberately NOT the default. A title that deadlocks under
 * real locks is telling you its threads are reaching a place the console's
 * would not, and collapsing the hierarchy hides that rather than answering it.
 * What it is good for is deciding whether a lock cycle is the last thing in the
 * way: if the title still does not get further with this on, the cycle was not
 * what was stopping it.
 *
 * Safe only because guest code here never holds a lock while waiting on another
 * guest thread to make progress -- the runtime's own I/O completes
 * synchronously. That would not hold in general.
    /* One lock, both sides, with the guest stack that got there.
     *
     * A take at one address and a release 0x78 lower is not a lock protocol
     * this runtime can reason about from addresses alone -- it needs the two
     * call sites, because the question is whether the guest computed different
     * addresses or we did. */
```
