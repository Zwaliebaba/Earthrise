// BloomBlurPS.hlsl — Gaussian blur pass for bloom (used for both horizontal and vertical).

cbuffer BlurConstants : register(b0)
{
    float2 TexelSize;
    float2 _Pad;
};

Texture2D SourceTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

static const float weights[5] = { 0.227027, 0.194946, 0.121622, 0.054054, 0.016216 };

float4 main(PSInput input) : SV_Target
{
    float3 result = SourceTexture.Sample(LinearSampler, input.TexCoord).rgb * weights[0];

    for (int i = 1; i < 5; ++i)
    {
        float2 offset = float2(TexelSize.x * i, TexelSize.y * i);
        result += SourceTexture.Sample(LinearSampler, input.TexCoord + offset).rgb * weights[i];
        result += SourceTexture.Sample(LinearSampler, input.TexCoord - offset).rgb * weights[i];
    }

    return float4(result, 1.0);
}
