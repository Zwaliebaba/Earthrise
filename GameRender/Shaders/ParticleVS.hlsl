// ParticleVS.hlsl — Vertex shader for instanced billboard particles.
// Each instance reads per-particle data from a StructuredBuffer and generates
// a camera-facing quad from SV_VertexID (6 vertices per quad, 2 triangles).

cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraRight;
    float    _Pad0;
    float3   CameraUp;
    float    _Pad1;
};

struct ParticleData
{
    float3 Position;    // Origin-rebased world position
    float  Size;
    float4 Color;
};

StructuredBuffer<ParticleData> Particles : register(t0);

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

static const float2 QuadOffsets[6] = {
    {-1,+1}, {+1,+1}, {-1,-1},
    {+1,+1}, {+1,-1}, {-1,-1}
};

VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    ParticleData p = Particles[instanceId];

    float2 offset = QuadOffsets[vertexId] * p.Size;
    float3 worldPos = p.Position
        + CameraRight * offset.x
        + CameraUp    * offset.y;

    output.Position = mul(float4(worldPos, 1.0), ViewProjection);
    output.Color = p.Color;
    return output;
}
