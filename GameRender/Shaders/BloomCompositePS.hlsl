// BloomCompositePS.hlsl — Composites bloom on top of the scene (scene + bloom).

Texture2D SceneTexture : register(t0);
Texture2D BloomTexture : register(t1);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float4 scene = SceneTexture.Sample(LinearSampler, input.TexCoord);
    float4 bloom = BloomTexture.Sample(LinearSampler, input.TexCoord);
    return float4(scene.rgb + bloom.rgb, scene.a);
}
