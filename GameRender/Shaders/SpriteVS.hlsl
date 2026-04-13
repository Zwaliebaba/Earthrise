// SpriteVS.hlsl — Vertex shader for SpriteBatch (flat-color 2D quads).

cbuffer SpriteConstants : register(b0)
{
    float4x4 Projection;
};

struct VSInput
{
    float2 Position : POSITION;
    float4 Color    : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(float4(input.Position, 0.0, 1.0), Projection);
    output.Color = input.Color;
    return output;
}
