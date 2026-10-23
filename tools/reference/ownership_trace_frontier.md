# Generic tracing review frontier

Reviewed recomp_trace.c in full at fork 4dc8b6f. The hooks implement generic
entry/exit/SP and INT 2D diagnostics. Caller-provided names/VAs and environment
controls select output; no title address table or captured payload was found.

Unresolved implementation defects from source inspection:

- Entry return address, stack arguments, pointed-to data, EAX dumps and guest
  ANSI_STRING headers/buffers use direct unchecked dereferences. Faulting or
  truncated guest memory can make the diagnostic itself fault. No deliberate
  crashing experiment was run in this checkpoint.
- Pointer dumps assume a 64 MB low guest range, which does not describe all
  configured backing, aliases or extended guest mappings.
- Process-wide profile counters/table, lazy environment initialization and
  budget state are unsynchronized across guest threads. Counts are not timing.
- Profile names are borrowed pointers; lifetime and shutdown/report quiescence
  are not enforced by this source.

The existing platform ReadProcessMemory implementation supports exact-size,
checked reads (process_vm_readv on Linux, mach_vm_read_overwrite on macOS).
Reuse the platform primitive for diagnostic reads, handle failed/partial reads
without direct fallback, and test guard-page/truncated memory on each host.
This is a generic mechanism defect, not a reason for a title workaround.
The ownership inventory remains needs_generic_fix for this source; no runtime,
fidelity or full ownership completion is claimed.


## Checked-read implementation

Fork 1b47454 replaces direct guest reads with the existing platform
ReadProcessMemory primitive. Reads validate the full 32-bit guest interval,
require exact byte counts, preserve host LastError, and never fall back to an
unchecked dereference. Dumps print unreadable markers; invalid debug strings
are skipped. Pointer selection no longer assumes low 64 MB backing.

The actual-source guard-page fixture crashed on Windows before the fix.
Plain/argument CTests pass 2/2 on Windows and 2/2 on Linux. They cover crossing
headers/strings/return addresses, invalid pointers, guest address overflow,
valid strings, high guest addresses and LastError preservation. The initial
Linux fixture build required replacing Windows GetSystemInfo with sysconf.
Both game builds pass. No macOS or live-game tracing acceptance is claimed.
Shared profile/budget initialization/state and borrowed names remain open;
needs_generic_fix stays in place for the source.


## Shared-state and name lifetime correction

Fork ed59e67 uses the existing native SRW lock for lazy profile/budget
state, table counts and periodic/exit reports. Reporting uses a locked helper
to avoid recursive lock acquisition. Profile names are copied at first entry
and retained with the process-lifetime table; caller buffers can be released.

The prior Windows fixture loses concurrent call counts and a mutable stack
name corrupts the stored name. Four CTests now pass on Windows and Linux:
checked reads, argument dumps, four-thread exact profile counts (200000 worker
calls plus one name snapshot), and exact budget admission (100000 of 200000
attempts). The profile fixture exercises periodic and exit reports. Both game
builds pass. Environment changes during tracing and signal/reentrant tracing
are unsupported; the global diagnostic lock may contend. No TSan, macOS or
live-game tracing proof. Recorded read/state/name defects are addressed; source
ownership is now retain_generic_boundary, not whole-runtime certification.
