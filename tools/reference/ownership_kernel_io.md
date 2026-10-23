# Kernel I/O ownership review

Full kernel_io.c source ownership read. Guest IRP initialization, native-pointer
fallbacks, symbolic links/object handles and unsupported-control diagnostics
are generic kernel/runtime mechanisms. No game ID, guest code address table or
captured title payload appears. Built-in D/T/U device-path names are standard
Xbox namespace defaults; title-specific host path translation is separate.

The shared IRP initializer validated the minimum header/stack layout but cleared
the whole supplied PacketSize. A packet larger than its minimum layout could
cross the 32-bit guest-address limit. The fix rejects that full-span overflow
before clearing the packet. Existing minimum-layout/end-of-stack validation is
preserved. A standalone bridge-test mode calls the shared implementation:
old code fails rejection and unchanged-buffer assertions; corrected code also
accepts an exact end-of-address-space packet. The host buffer is independently
sized so the test does not rely on an out-of-bounds native allocation.

Remaining gaps: native device/dispatch/synchronous I/O paths are unsupported,
IoMarkIrpMustComplete is a no-op, symbolic links and object handles use fixed
unlocked tables with truncation and stale-handle risks, and unsupported-control
notes retain caller string pointers. Native pointer-to-guest conversion and
IRP buffer writability remain caller responsibilities requiring wider review.
Source remains needs_generic_fix. This is not full I/O semantics, hardware or
gameplay certification.

Checkpoint fork: 2da4407f02eb378f4f5f8b0d0045a31e371b23eb. Windows bridge suite passes 45/45 and both game
builds pass. Linux boundary test is pending a fresh dependency build; no Linux
pass is claimed for this checkpoint.
