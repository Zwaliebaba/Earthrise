// FlatColorPS.hlsl — Flat-color pixel shader
// Flat diffuse color from per-draw constant buffer + basic ambient/directional light.

cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraPosition;
    float    _Pad0;
    float3   LightDirection;
    float    AmbientIntensity;
};

cbuffer DrawConstants : register(b1)
{
    float4x4 World;
    float4   Color;
};

struct PSInput
{
    float4 Position : SV_Position;
    nointerpolation float3 WorldNormal : NORMAL;
};

float4 main(PSInput input) : SV_Target
{
    // Basic directional light for depth perception
    float NdotL = saturate(dot(input.WorldNormal, -LightDirection));
    float light = AmbientIntensity + (1.0 - AmbientIntensity) * NdotL;

    return float4(Color.rgb * light, Color.a);
}
