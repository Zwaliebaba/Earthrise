// BillboardVS.hlsl — Vertex shader for camera-facing billboard quads (e.g., sun glow).

cbuffer BillboardCB : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraRight;
    float    Radius;
    float3   CameraUp;
    float    _Pad0;
    float3   Center;
    float    _Pad1;
    float4   Color;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

static const float2 QuadUVs[6] = {
    {0,0}, {1,0}, {0,1},
    {1,0}, {1,1}, {0,1}
};
static const float2 QuadOffsets[6] = {
    {-1,+1}, {+1,+1}, {-1,-1},
    {+1,+1}, {+1,-1}, {-1,-1}
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    float2 offset = QuadOffsets[vertexId] * Radius;
    output.TexCoord = QuadUVs[vertexId];

    float3 worldPos = Center
        + CameraRight * offset.x
        + CameraUp    * offset.y;

    output.Position = mul(float4(worldPos, 1.0), ViewProjection);
    return output;
}
