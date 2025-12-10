Texture2D gTex : register(t0);

// Comment in English: Four individual static samplers (matched with root signature s0..s3).
SamplerState gSamplerLinearWrap : register(s0);
SamplerState gSamplerPointWrap : register(s1);
SamplerState gSamplerLinearClamp : register(s2);
SamplerState gSamplerPointClamp : register(s3);

// Comment in English: Constant buffer shared with C++.
cbuffer CbMvp : register(b0)
{
    float4x4 gMVP; // Not used in PS, but layout must match C++ side.
    uint gSamplerIndex; // Which sampler to use (0..3).
    float3 _padding; // Padding for 16-byte alignment.
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD;
};

float4 main(PSInput i) : SV_Target
{
    uint idx = gSamplerIndex;

    float4 tex;

    // Comment in English: Select the correct sampler based on index.
    if (idx == 0)
    {
        tex = gTex.Sample(gSamplerLinearWrap, i.uv);
    }
    else if (idx == 1)
    {
        tex = gTex.Sample(gSamplerPointWrap, i.uv);
    }
    else if (idx == 2)
    {
        tex = gTex.Sample(gSamplerLinearClamp, i.uv);
    }
    else
    {
        tex = gTex.Sample(gSamplerPointClamp, i.uv);
    }

    return lerp(tex, float4(i.color, 1.0f), 0.25f);
}
