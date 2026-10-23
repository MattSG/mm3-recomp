# Mip-completion ownership frontier

Status: unresolved at fork b59fbc5. This is a finding, not a migrated policy.

`host_levels` automatically expands a partial non-volume mip chain to 1x1.
The caller's declared guest levels still bound guest memory reads, but host
resources and generated sampling levels differ. A direct production-helper
probe on a 32x32 texture supplied with two levels reports:

```text
guest_levels=2 host_levels=6
```

Both D3D11 and SDL GPU `texture_get` call this helper. They generate missing
levels with the shared box reduction. Short chains can also select a different
host format because native BC uploads require a complete supplied chain.
This is a visual enhancement; it is not merely format/address translation.

The native and SDL cache paths can return on matching metadata/epoch/hash
before computing host levels. A future policy must therefore be fixed before
cache use, or participate in cache identity/invalidation; changing a callback
alone would not update existing resources.

Required migration: preserve supplied guest mip counts by default. Expose an
explicit generic opt-in for completing supported 2D/cube chains and let MM3
select that mode before texture creation. Keep layout/decoding, checked uploads,
reduction and retry mechanics shared. Preserve the current MM3 behavior when
selected; single-level and volume limitations remain explicit.

Verification must cover default counts and native formats, opted-in generated
mips with real GPU readback, failed/retried uploads, guest bounds, both native
and SDL backends, explicit Vulkan/D3D12 runs and a bounded MM3 driving check.
Shared mip-generation tests must select the enhancement explicitly rather than
require it as an implicit default. Inspect the existing native-BC fallback
expectations and CPU helper calls as well. No live visual-change claim is made.

## Existing shared policy

```c
/* Mip levels the host texture gets. Titles shipped short chains to save
 * memory (for example, 1024x1024 with 4 levels), so distant surfaces kept sampling
 * a level far finer than their footprint -- grain and shimmer, with a band
 * where the chain runs out. A chain the title started is completed down to
 * 1x1 from its own smallest level; a single level stays single. */
static int host_levels(const TexLayout *L, const NvD3DTexture *t)
{
    int n = 1;
    uint32_t m = t->width > t->height ? t->width : t->height;
    if (tex_volume(t) || L->levels < 2) return L->levels;
    while (m > 1) { m >>= 1; n++; }
    return n > L->levels ? n : L->levels;
}

```
