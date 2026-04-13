// ParticlePS.hlsl — Pixel shader for billboard particles.
// Outputs per-instance color with alpha for additive blending.

struct PSInput
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

float4 main(PSInput input) : SV_Target
{
    return input.Color;
}
