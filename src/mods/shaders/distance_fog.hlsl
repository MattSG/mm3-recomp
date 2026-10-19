/* MM3 extended-distance air and destination-alpha horizon composition. */
#ifdef SDLGPU
#include "nv2a_bindings.hlsli"
PS_SAMP(DepthS, 0);
#else
#define CBUF(n) register(b##n)
#define PS_TEX(type, name, n) type name : register(t##n)
#endif
cbuffer Fog : CBUF(0) { float4 band; float4 colour; float4 sky; };
#if FOG_MSAA
Texture2DMS<float> Depth : register(t0);
#else
PS_TEX(Texture2D<float>, Depth, 0);
#endif
float4 fog_vs(uint id : SV_VertexID) : SV_Position {
  float2 p = float2((id << 1) & 2, id & 2);
  return float4(p * float2(2,-2) + float2(-1,1), 0, 1);
}
#if FOG_MSAA
float4 fog_ps(float4 p : SV_Position, uint sample : SV_SampleIndex) : SV_Target {
#else
float4 fog_ps(float4 p : SV_Position) : SV_Target {
#endif
#if FOG_MSAA
  float z = Depth.Load(int2(p.xy), sample);
#else
  float z = Depth.Load(int3(int2(p.xy), 0));
#endif
  if (colour.a > 0) {
    float a = z >= 0.99999 ? 1 : 0;
    if (sky.w > 0) {
      float t = saturate(dot(float3(p.xy,1),sky.xyz) * sky.w);
      a *= t*t*(3-2*t);
    }
    return float4(0,0,0,a);
  }
  if (z >= 0.99999) return float4(colour.rgb,band.w);
  float x = saturate((z * band.z - band.x) * band.y);
  return float4(colour.rgb, 1.0 - exp2(-6.643856 * x * x));
}
