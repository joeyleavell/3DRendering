struct PSIn
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

struct PSOut
{
    float4 Color       : SV_Target0;
};

Texture2D<float4>  Tex : register(t0, space0);
SamplerState      Samp : register(t0, space0);

PSOut main(PSIn Input)
{
    PSOut Output;

    float4 Color = Tex.Sample(Samp, Input.UV);
    Output.Color = Color;

    return Output;
}