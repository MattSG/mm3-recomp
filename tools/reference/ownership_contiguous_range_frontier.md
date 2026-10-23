# Generic bounded contiguous allocation

The guest MmAllocateContiguousMemoryEx bridge previously treated a narrow
low/high range as an implicit pinned allocation. A Halo-derived heuristic
zeroed and returned the requested physical address without allocator ownership,
range exclusion or duplicate checking. The original branch/rationale lives in
legacy_pinned_contiguous_policy.md.

The generic extension xbox_ContiguousAllocRange takes inclusive physical
bounds and uses the existing sorted allocation table, heap exclusion, free
extent splitting, page rounding and zeroing. Exact requests succeed only when
that aligned page-rounded span is available. Unavailable/invalid intervals fail.
The original unconstrained API delegates to the new function with full bounds.
The guest bridge uses this mechanism for every low/high request; no pinned
heuristic or title-owned pool data remains in that function.

Validation:

- Original bridge linked against the new fixture reproduces duplicate issuance
  of an already-owned exact range. Source was restored and the check passed.
- Actual allocator/bridge fixture checks exact ranges, duplicate refusal, invalid
  bounds/alignment, insufficient rounded size, high overflow, splitting inside
  a freed extent with untouched neighbors, zeroing and heap exclusion.
- Full kernel suites pass 64 Windows and 63 Linux tests; both game builds pass.
  Expanded guest-bridge assertions are checked separately after the suite run.

Limits: guest protection flags are still ignored in this bridge; native typed
host allocation wrappers are separate and remain partial. This does not certify
physical allocator parity, unsupported page-zero requests, installed-RAM size
policy, or gameplay. Broader kernel bridge/image ownership remains under review.
