struct VSIn
{
    float2 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

VSOut main(VSIn Input)
{
    VSOut Output;

    Output.Position = float4(Input.Position, 0.0, 1.0);
    Output.UV = Input.UV;

    return Output;
}