cbuffer CbMvp : register(b0)
{
    float4x4 gMVP;
}

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};
struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

PSInput main(VSInput input)
{
    PSInput o;
    o.position = mul(gMVP, float4(input.position, 1.0f));
    o.color = input.color;
    return o;
}

