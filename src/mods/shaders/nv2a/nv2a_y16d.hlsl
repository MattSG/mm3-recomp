/* The same view of a Z24S8 depth buffer (fog passes read depth this way).
 * Guest memory holds z[7:0]:stencil in texel 2x and z[23:8] in 2x+1; a
 * pixel-centred guest tap lands on 2x+1 exactly, which host-resolution
 * rounding splits between the two (moire in the fog). Both texels hold
 * z[23:8]. ponytail: the low word (z[7:0]:stencil) is not readable this way;
 * keep it for even texels if a title ever reads stencil through Y16.
 * A high-word tap sits a quarter guest pixel to the right of the shaded
 * pixel. ccol.z supplies host texels per guest pixel. Shift the depth read
 * left by a quarter guest pixel so a building's
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
  int zx = max(0, (int)floor((q.x + 0.5) * 0.5 - 0.25 * ccol.z));
#ifdef Y16_MS
  float d = Y16Z.Load(int2(zx, q.y), 0);
#else
  float d = Y16Z.Load(int3(zx, q.y, 0));
#endif
  uint word = (uint)round(saturate(d) * 16777215.0) >> 8;
  return float4(1, (word >> 8) / 255.0, (word & 255u) / 255.0, 1);
}
