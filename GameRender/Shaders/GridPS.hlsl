// GridPS.hlsl — Pixel shader for tactical grid lines.

struct PSInput
{
    float4 Position : SV_Position;
    float  Alpha    : ALPHA;
};

float4 main(PSInput input) : SV_Target
{
    // Cyan/blue grid lines
    return float4(0.1, 0.6, 0.8, input.Alpha * 0.5);
}
