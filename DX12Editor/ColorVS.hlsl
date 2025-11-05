cbuffer CbMvp : register(b0) // new: bound from CPU as CBV at b0
{
    float4x4 gMVP;
}
// Simple vertex shader: transforms vertex and passes color
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
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

