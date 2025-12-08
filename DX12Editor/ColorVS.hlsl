cbuffer CbMvp : register(b0)
{
    float4x4 gMVP;
}

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD;
};
struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD;
};
PSInput main(VSInput i)
{
    PSInput o;
    o.position = mul(gMVP, float4(i.position, 1));
    o.color = i.color;
    o.uv = i.uv;
    return o;
}


