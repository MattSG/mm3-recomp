# Depth-view ownership frontier

Inspected fork: `2218bff0b2d482a15a300e9530b4d0297d092c06`. This is an unresolved source finding, not a GPU
fidelity result or an implemented migration.

`nv2a_y16d.hlsl` converts sampled depth to a 24-bit integer and returns bits
23:8 for every output texel. Output x selects a source depth pixel, but never
selects the low or high 16-bit word of packed Z24S8. Its comment explicitly
states that the low word/stencil is not readable through this representation.
Both renderers additionally install a fixed 0.25 guest-pixel left shift.
The paired colour Y16 shader, by comparison, chooses distinct low/high words
using output-x parity. This is a meaningful format/view distinction.

The current shader and backend paths therefore cannot be certified as complete
generic Z24S8-to-Y16 reinterpretation. They combine a partial high-word proxy
with alignment chosen to suppress the captured haze/moiré symptom. The shared
renderer must not silently impose an application's approximation on every port.

Existing port render-policy hooks cover forced depth, cubic filtering, idle
presentation and vertex alignment. MM3's implementations are in
`src/mods/render_policy.c`, registered in `src/mods/widescreen_hud.c`.
They currently do not select this depth-view approximation. A migration must
preserve any required MM3 compatibility explicitly while giving the shared
runtime a truthful generic view contract. Exact low-word/stencil support or an
explicitly selected proxy remains implementation work; it has not been decided
or supplied by this audit checkpoint.

Existing surface-sync CMake cases cover uploads, retries, dirty data, scanout
and related synchronization. The inspected lists contain no direct GPU check
for depth-Y16 word parity or stencil preservation. Add actual readback checks
with distinguishable high/low depth bytes and stencil before claiming format
fidelity. Compile success or a night-driving screenshot cannot prove it.

## Evidence snapshot

- `src/video/shaders/nv2a_y16d.hlsl` SHA-256 `902da1a4072fac0b9800efd0c7e7697267b1dd44a9dfd15c67234f37d44c93fd`
- `src/video/nv2a_d3d11.c` SHA-256 `1327edceec92598c83bce6bc5931d787d8138c74aeaa2feba1922d236288a842`
- `src/video/nv2a_sdlgpu.c` SHA-256 `7a7d94b8b1cb7b2289d115f4940e3549ed3e3504c1602ff686397a06a0409a99`

## Current depth conversion shader

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
