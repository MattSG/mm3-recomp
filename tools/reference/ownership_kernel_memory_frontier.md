# Kernel native memory wrapper review

Full kernel_memory.c source read at fork ed59e67. These are generic native-host
wrappers, exposed by typed thunks; the guest ABI bridge has separate paths for
several allocations. MmQueryStatistics is called from the bridge as well.
No title address table or application-specific allocator data was found.

Unresolved semantics from source inspection:

- AvailablePages comes from host availability, capped by a half-total fallback;
  it does not measure guest allocations. Other accounting fields are zero.
- ContiguousMemoryEx ignores low/high bounds; aligned_malloc storage does not
  apply requested protection, and VirtualAlloc fallback may miss alignment.
- FreeContiguousMemory infers allocation type using VirtualQuery rather than
  owned allocation metadata. Allocation/query/protect native wrappers do not
  establish full guest ABI/layout semantics.
- Persist/lock/instance-memory helpers remain partial/stubbed; kernel-stack
  wrappers are separate from owned guest stack paths used in the bridge.

These are generic implementation gaps, not reasons for title workarounds.
Ownership remains needs_generic_fix; focused stats tests do not certify the
whole file. The configured total-RAM correction and its validation are recorded
in the current ownership checkpoint.


## Configured RAM correction

Fork d9adc17 reports TotalPhysicalPages from existing g_xbox_total_ram
instead of a literal 64 MB. Prior 128 MB test fails; configured 64/128 MB,
null input, returned size and bounded availability pass Windows/Linux. Both
game builds pass. Native wrapper assumptions were moved to
legacy_native_memory_assumptions.md and replaced with explicit partial-contract
comments. This does not resolve the availability/alignment/ownership gaps above.
