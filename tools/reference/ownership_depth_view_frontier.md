# Depth-view ownership record

Historical finding inspected at fork `2218bff0b2d482a15a300e9530b4d0297d092c06`.
The snapshot below is retained as evidence; it is not the current shared shader.

## Resolved policy ownership

MM3 owns its high-word-only depth proxy and quarter-guest-pixel alignment in
`src/mods/shaders/nv2a/nv2a_y16d.hlsl`. The shared copy has been removed.
`XBOXRECOMP_NV2A_SHADER_DIR` selects an optional caller converter for both
D3D11 and SDL GPU; only configured builds compile the converter. Shared backends
own target allocation, scale metadata, execution and cached view lifetime.
They do not supply implicit high-word duplication or alignment policy.

Without a converter, a cached-depth-as-Y16 draw reports the missing conversion
and is skipped, without allocating a proxy or falling back to stale RAM.
Source-selection checks, actual absent-converter cases and configured GPU
readback/retry cases establish this ownership boundary. They do not establish
complete native packed-depth/stencil format emulation.

## Remaining fidelity boundary

The MM3 converter preserves the inherited visual approximation; it cannot read
the packed low depth/stencil word. Full generic Z24S8-to-Y16 emulation remains
unimplemented. Proving that future feature requires actual low/high word and
stencil readback checks, not a screenshot or compile-only result. Other shader
and renderer ownership scopes remain subject to the wider audit.

## Evidence snapshot

- `src/video/shaders/nv2a_y16d.hlsl` SHA-256 `902da1a4072fac0b9800efd0c7e7697267b1dd44a9dfd15c67234f37d44c93fd`
- `src/video/nv2a_d3d11.c` SHA-256 `1327edceec92598c83bce6bc5931d787d8138c74aeaa2feba1922d236288a842`
- `src/video/nv2a_sdlgpu.c` SHA-256 `7a7d94b8b1cb7b2289d115f4940e3549ed3e3504c1602ff686397a06a0409a99`

## Historical shared depth conversion shader

```hlsl
/* The same view of a Z24S8 depth buffer (fog passes read depth this way).
 * Guest memory holds z[7:0]:stencil in texel 2x and z[23:8] in 2x+1; a
 * pixel-centred guest tap lands on 2x+1 exactly, which host-resolution
 * rounding splits between the two (moire in the fog). Both texels hold
 * z[23:8]. ponytail: the low word (z[7:0]:stencil) is not readable this way;
 * keep it for even texels if a title ever reads stencil through Y16.
 * A high-word tap sits a quarter guest pixel to the right of the shaded
 * pixel. Shift the depth read left by ccol.z host pixels so a building's
 * edge does not accidentally use sky depth and acquire a bright haze rim.
 * Host depth is z / 0xFFFFFF (see map[2]). */
#ifdef Y16_MS
Texture2DMS<float> Y16Z : register(t0);
#else
PS_TEX(Texture2D<float>, Y16Z, 0);
#endif
#ifdef SDLGPU
PS_SAMP(Y16ZS, 0);
#endif
float4 y16d_ps(float4 p : SV_Position) : SV_Target {
  uint2 q = uint2(p.xy);
  int zx = max(0, (int)floor((q.x + 0.5) * 0.5 - ccol.z));
#ifdef Y16_MS
  float d = Y16Z.Load(int2(zx, q.y), 0);
#else
  float d = Y16Z.Load(int3(zx, q.y, 0));
#endif
  uint word = (uint)round(saturate(d) * 16777215.0) >> 8;
  return float4(1, (word >> 8) / 255.0, (word & 255u) / 255.0, 1);
}
```
