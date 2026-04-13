// StarfieldPS.hlsl — Pixel shader for starfield points.

struct PSInput
{
    float4 Position   : SV_Position;
    float  Brightness : BRIGHTNESS;
};

float4 main(PSInput input) : SV_Target
{
    return float4(input.Brightness, input.Brightness, input.Brightness * 0.95, 1.0);
}
