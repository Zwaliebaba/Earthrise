// BillboardPS.hlsl — Pixel shader for textured billboard quads (e.g., sun glow).

Texture2D GlowTexture : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer BillboardCB : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraRight;
    float    Radius;
    float3   CameraUp;
    float    _Pad0;
    float3   Center;
    float    _Pad1;
    float4   Color;
};

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float4 texel = GlowTexture.Sample(LinearSampler, uv);
    return float4(texel.rgb * Color.rgb, texel.a * Color.a);
}
