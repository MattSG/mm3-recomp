# D3D8 combiner cache failure ownership

Scoped source review of the combiner header and shader compilation/cache/draw
integration at fork 8ac688973147872e38374c7b4feda62137c28222. The state-driven translator and cache are
generic rendering mechanisms. Unsupported population/sizing claims moved to
legacy_d3d8_combiner_cache_claims.md. The entire translation implementation was
not certified by this scoped review.

A full cache released the victim shader before compiling its replacement.
If CreatePixelShader failed, the cache retained the released shader pointer and
old identity. Compilation now succeeds before cache ownership is changed.

The existing WARP A8 fixture fills all 128 slots, retains independent shader
references, injects CreatePixelShader failure through a device accessor, checks
unchanged reference counts and cache hits, then retries successful replacement.
Old code fails shader ownership comparison; corrected code passes alongside
448 texture and four RHW interpolation draws. D3D11 game rebuild passes.
This is a Windows D3D11 mechanism; no Vulkan/native hardware result is claimed.

Broader gaps remain: cache identity includes constants/raw struct bytes, clock
wrap/eviction policy, generation input validation, RGB/alpha stage ordering,
constant upload failure handling and fixed-function fallback semantics need
focused review. No full combiner fidelity or gameplay certification is made.
