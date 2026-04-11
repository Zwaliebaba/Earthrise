// FlatColorPS.hlsl — Pixel shader for lit flat-color rendering.
// Hemisphere ambient, half-Lambert diffuse, Blinn-Phong specular, fresnel rim.

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
    float4   Color;
};

struct PSInput
{
    float4 Position      : SV_Position;
    float3 WorldNormal   : NORMAL;
    float3 WorldPosition : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float3 N = normalize(input.WorldNormal);
    float3 L = normalize(-LightDirection);
    float3 V = normalize(CameraPosition - input.WorldPosition);
    float3 H = normalize(L + V);

    // Hemisphere ambient: brighter from above (sky), darker below (ground)
    float skyBlend = N.y * 0.5 + 0.5;
    float ambient  = lerp(0.25, 0.65, skyBlend) * AmbientIntensity;

    // Half-Lambert diffuse: softer shadow falloff than standard Lambert
    float NdotL       = dot(N, L);
    float halfLambert = NdotL * 0.5 + 0.5;
    float diffuse     = halfLambert * halfLambert;

    // Blinn-Phong specular highlight
    float NdotH = saturate(dot(N, H));
    float spec  = pow(NdotH, 40.0) * 0.25 * step(0.0, NdotL);

    // Fresnel rim for silhouette readability in space
    float NdotV = saturate(dot(N, V));
    float rim   = pow(1.0 - NdotV, 3.0) * 0.12;

    float3 result = Color.rgb * (ambient + diffuse * (1.0 - AmbientIntensity))
                  + spec
                  + rim * Color.rgb;

    return float4(result, Color.a);
}
