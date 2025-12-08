Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD;
};

float4 main(PSInput i) : SV_Target
{
    float4 tex = gTex.Sample(gSamp, i.uv);
    return lerp(tex, float4(i.color, 1), 0.25);
}

