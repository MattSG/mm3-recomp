# Kernel RTL source ownership review

Full kernel_rtl.c source read at fork 1346a31bbbbdb69b3f2d7a7a289e753b9ae491a4. String, memory, status,
time, formatting and critical-section services are generic runtime mechanisms.
Critical-section shadow state is keyed by caller addresses. CRT table metadata
is supplied through xbox_SetCrtLockTable rather than hardcoded per title.
Environment-driven tracing and shared-lock mode are optional diagnostics.

Captured Half-Life 2 failures and a specific lock-address offset observation
moved to legacy_rtl_lock_observations.md. The shared-lock comment now correctly
states that cross-thread waits can still deadlock. A before/after comparison
with block comments and whitespace removed confirms executable source tokens
are unchanged. No new runtime test is claimed for this documentation migration.

Remaining generic gaps: guest backtrace/CRT table scans dereference unchecked
guest addresses with 32-bit arithmetic; diagnostic lazy flags/owner snapshots
have concurrency concerns; shadow table exhaustion returns unprotected locks;
shadows are permanent and address reuse has no retirement protocol. String
length truncation, codepage sizing, counted-string comparisons, host exception
ABI delegation and incomplete status mappings need focused correctness review.
These are not title policy and cannot be resolved by moving them to MM3. Source
remains needs_generic_fix; this review does not certify whole RTL correctness.
