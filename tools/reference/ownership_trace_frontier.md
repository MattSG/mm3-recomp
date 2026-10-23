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
