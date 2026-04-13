// BloomExtractPS.hlsl — Extracts bright regions from the scene for bloom processing.

cbuffer BloomConstants : register(b0)
{
    float Threshold;
    float3 _Pad;
};

Texture2D SceneTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float4 color = SceneTexture.Sample(LinearSampler, input.TexCoord);
    float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    float contribution = max(0, brightness - Threshold);
    return float4(color.rgb * (contribution / max(brightness, 0.001)), 1.0);
}
