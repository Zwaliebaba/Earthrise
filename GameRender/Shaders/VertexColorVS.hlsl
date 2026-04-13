// VertexColorVS.hlsl — Vertex shader for landscape-colored rendering.
// Single-pass with barycentric wireframe. Transforms position, computes per-vertex
// lighting, and passes barycentric coordinates (encoded in TexCoord0) for edge
// detection in the pixel shader.

cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraPosition;
    float    _Pad0;
    float3   LightDirection;
    float    AmbientIntensity;
};

cbuffer DrawConstants : register(b1)
{
    float4x4 World;
};

struct VSInput
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float4 Color     : COLOR;
    float2 TexCoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 Position  : SV_POSITION;
    float4 Color     : COLOR0;
    float2 Bary      : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Transform position
    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    output.Position = mul(worldPos, ViewProjection);

    // Pass barycentric coordinates for wireframe edge detection
    output.Bary = input.TexCoord0;

    // ---- Lighting (always on for landscape) ----
    float3 normal = normalize(mul(input.Normal, (float3x3) World));

    // Color-material: vertex colour drives ambient + diffuse
    float4 matAmbient = input.Color;
    float4 matDiffuse = input.Color;

    float3 ambient = AmbientIntensity * matAmbient.rgb;

    // Simple directional light
    float3 lightDir = normalize(-LightDirection);
    float NdotL = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = NdotL * matDiffuse.rgb;

    float3 finalColor = ambient + diffuse;
    output.Color = float4(saturate(finalColor), matDiffuse.a);

    return output;
}
