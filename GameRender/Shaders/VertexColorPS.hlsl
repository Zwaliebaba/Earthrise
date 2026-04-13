// VertexColorPS.hlsl — Pixel shader for landscape-colored rendering with barycentric wireframe.
//
// Uses interpolated barycentric coordinates to detect triangle edges.
// Pixels near any edge (where a barycentric component approaches 0)
// receive an additive highlight tinted by the surface colour,
// reproducing the Darwinia triangle-outline aesthetic.

struct PSInput
{
    float4 Position  : SV_POSITION;
    float4 Color     : COLOR0;
    float2 Bary      : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    // Base terrain colour (vertex-lit)
    float3 baseColor = input.Color.rgb;

    // Reconstruct 3-component barycentrics from the 2 interpolated values
    float3 bary = float3(input.Bary, 1.0 - input.Bary.x - input.Bary.y);

    // Screen-space anti-aliased wireframe: use fwidth for consistent line width
    float3 fw = fwidth(bary);
    float3 edgeFactor = smoothstep(float3(0,0,0), fw * 1.2, bary);
    float edge = 1.0 - min(edgeFactor.x, min(edgeFactor.y, edgeFactor.z));

    // Additive edge highlight tinted by surface colour (Darwinia style)
    float3 color = baseColor + edge * baseColor;

    return float4(color, 1.0f);
}
