// FontPS.hlsl — Pixel shader for BitmapFont glyph rendering (textured + tinted).

Texture2D FontTexture : register(t0);
SamplerState PointSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
    float4 Color    : COLOR;
};

float4 main(PSInput input) : SV_Target
{
    float4 texel = FontTexture.Sample(PointSampler, input.TexCoord);
    // Use texture alpha for glyph shape, multiply RGB by tint color
    return float4(input.Color.rgb * texel.rgb, input.Color.a * texel.a);
}
