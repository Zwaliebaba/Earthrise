// FlatColorVS.hlsl — Flat-color vertex shader
// MVP transform with nointerpolation for flat shading.

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

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
};

struct VSOutput
{
    float4 Position : SV_Position;
    nointerpolation float3 WorldNormal : NORMAL;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(float4(input.Position, 1.0), World);
    output.Position = mul(worldPos, ViewProjection);
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) World));

    return output;
}
