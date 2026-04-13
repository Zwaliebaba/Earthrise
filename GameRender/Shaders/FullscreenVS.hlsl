// FullscreenVS.hlsl — Full-screen triangle vertex shader (no vertex buffer).
// Generates a single triangle that covers the entire screen using SV_VertexID.

struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    // Full-screen triangle: 3 vertices, no vertex buffer needed
    output.TexCoord = float2((vertexId << 1) & 2, vertexId & 2);
    output.Position = float4(output.TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}
