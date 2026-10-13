# Runtime patch integration — 2026-10-13

`tools/xboxrecomp` and `scripts/apply_runtime_patch.py` now pin public runtime
`659963503c39c18a8e30ca4e7ef43004fac2da51`. The patch retains all 102 changed or
new files from the original fork runtime based on `12cb46d`.

The integration preserves the fork's audio/DSP, input, memory, threading,
timers, file diagnostics, graphics, shader-cache and pacing changes, together
with Matt's newer secure LAN cryptography, shared reads, Insignia DNS routing
and `RECOMP_XBOX_HDKEY`, `RECOMP_XBOX_SIGKEY`, `RECOMP_XBOX_LANKEY` settings.
The crypto and network implementations are unchanged from the public base.

Read-only guest opens share reads for two system-link instances, including
when the guest requests zero share access. Write opens retain their requested
sharing policy, and real host locks still return `STATUS_SHARING_VIOLATION`.
A regression covers both behaviors alongside the fork's thread-local file errors and guest error
conversion. The shader-prewarm harness now reserves a unique temporary path
to avoid collisions with directories left by an earlier process using the
same Windows process ID.

An independent clean toolkit clone verified patch application, a second
idempotent application, exact reproduction of all 102 patched files after
line-ending normalization, reverse application restoring the pristine tree,
and rejection of the older `e9121ba` base. Source whitespace checks passed
with the runtime's existing CRLF endings recognized.

The following native Release suites were rebuilt and run against the final
`6599635` base:

| Suite | Passed |
| --- | ---: |
| Kernel dispatch, file sharing, timers and synchronization | 57 |
| Kernel regressions, including DES and modular exponentiation | 1 |
| Network, including Insignia DNS | 1 |
| Pixel-shader prewarm and cache persistence | 10 |
| Graphics readback and retry paths | 11 |
| **Total on final public base** | **80** |

Another 119 cases passed before the final public-base update in unchanged
audio/DSP, keyboard, save-path, memory and graphics suites. These included
bindings, flare depth, graphics quality, hashing, pacing, samplers, shader
cache/lookup, surface synchronization, render targets and textures. Across
the integration, 199 distinct regression cases passed. The previous prewarm
temporary-directory failure is resolved. These checks use synthetic inputs;
they do not establish an end-to-end Xbox Live sign-in or system-link join.

The patched runtime remains an uncommitted submodule working tree. Its complete
source and regression changes are delivered through
`patches/xboxrecomp-mm3.patch`, applied by `python scripts/apply_runtime_patch.py`.
