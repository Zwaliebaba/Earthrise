// StarfieldVS.hlsl — Vertex shader for starfield point rendering.

cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
};

struct VSInput
{
    float3 Direction  : POSITION;
    float  Brightness : BRIGHTNESS;
};

struct VSOutput
{
    float4 Position   : SV_Position;
    float  Brightness : BRIGHTNESS;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    // Place star on a large sphere (far but within draw distance)
    float3 worldPos = input.Direction * 19000.0;
    output.Position = mul(float4(worldPos, 1.0), ViewProjection);
    output.Brightness = input.Brightness;
    return output;
}
