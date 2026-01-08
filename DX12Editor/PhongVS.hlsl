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
    float _pad4;
};

struct VSInput
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 posH : SV_POSITION;
    float3 normalW : NORMAL;
    float3 worldPos : TEXCOORD0;
};

VSOutput main(VSInput i)
{
    VSOutput o;

    float4 worldPos = mul(float4(i.pos, 1.0f), gWorld);
    o.worldPos = worldPos.xyz;

    // Normal transform (assumes uniform scale for assignment).
    o.normalW = normalize(mul(float4(i.norm, 0.0f), gWorld).xyz);

    o.posH = mul(worldPos, gViewProj);
    return o;
}
