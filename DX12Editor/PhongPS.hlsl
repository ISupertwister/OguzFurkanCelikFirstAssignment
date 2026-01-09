cbuffer CBPhong : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;

    float3 gLightDir;
    float _pad0;
    float3 gLightColor;
    float _pad1;
    float3 gAmbientColor;
    float _pad2;

    float3 gDiffuseColor;
    float _pad3;
    float3 gSpecularColor;
    float gShininess;

    float3 gCameraPos;
    float _pad4;
};

struct PSInput
{
    float4 posH : SV_POSITION;
    float3 normalW : NORMAL;
    float3 worldPos : TEXCOORD0;
};

float4 main(PSInput i) : SV_TARGET
{
    float3 N = normalize(i.normalW);
    float3 L = normalize(-gLightDir); // Directional light
    float3 V = normalize(gCameraPos - i.worldPos);

    float NdotL = max(dot(N, L), 0.0f);

    float3 diffuse = gDiffuseColor * NdotL;

    // Blinn-Phong
    float3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0f), max(gShininess, 1.0f));
    float3 specular = gSpecularColor * spec;

    float3 color = gAmbientColor + (diffuse + specular) * gLightColor;
    return float4(color, 1.0f);
}
