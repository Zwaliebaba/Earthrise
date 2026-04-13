// GridVS.hlsl — Vertex shader for tactical grid with distance fade.

cbuffer GridConstants : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraPosition;
    float    GridY;
    float    GridExtent;
    float    FadeStart;
    float    FadeEnd;
    float    _Pad;
};

struct VSInput
{
    float3 Position : POSITION;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float  Alpha    : ALPHA;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float3 worldPos = float3(input.Position.x, GridY, input.Position.z);
    output.Position = mul(float4(worldPos, 1.0), ViewProjection);

    // Distance fade
    float dist = length(worldPos - CameraPosition);
    output.Alpha = 1.0 - saturate((dist - FadeStart) / (FadeEnd - FadeStart));

    return output;
}
