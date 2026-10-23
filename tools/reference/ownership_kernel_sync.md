# Native kernel synchronization ownership

Full src/kernel/kernel_sync.c source read at fork 44acaede094d0390f5e6c445926eedb2519d5a13. Events,
semaphores, waits, mutexes, native timers and callback DPCs are generic runtime
services. Object pointers and callbacks are caller-supplied; no title IDs,
captured guest addresses, per-title lock tables or game-specific scheduling
presets appear. Retain the native service boundary in XboxRecomp.

This source is distinct from guest-address synchronization in kernel_bridge.c.
Guest-bridge timer/DPC/exclusion test results must not be used as proof for
these native-pointer implementations. No behavior changed and no fresh runtime
test is claimed for this source-only ownership review.

Remaining generic correctness gaps:

- Native wait and timer conversions negate INT64_MIN and narrow long finite
  millisecond intervals to DWORD, risking wrap or accidental INFINITE; positive
  sub-millisecond deadlines can also become polls. Correct long-duration waits
  need deadline-preserving chunking, not simply clamping finite waits.
- Native timer queue initialization is unsynchronized, cancellation/rearm and
  callback access race over caller-owned fields, and event/timer ownership has
  no complete teardown protocol.
- Native DPC submission shares mutable callback arguments and cancellation
  always returns FALSE; it does not provide guest queue/IRQL semantics.
- KeSynchronizeExecution directly calls a native callback without interrupt
  exclusion; previous event/mutant states are placeholders. Event/wait type,
  namespace/attributes and abandoned-wait status handling are partial.

These are generic mechanism gaps, not application policy to relocate. Source
remains needs_generic_fix. No full synchronization or hardware-equivalence
claim follows from this ownership decision.
