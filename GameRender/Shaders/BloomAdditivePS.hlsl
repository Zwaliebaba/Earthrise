// BloomAdditivePS.hlsl — Outputs bloom texture for additive blending.

Texture2D BloomTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float4 bloom = BloomTexture.Sample(LinearSampler, input.TexCoord);
    return bloom;
}
