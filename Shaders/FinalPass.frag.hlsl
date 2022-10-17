struct PSIn
{
    float4 Position : SV_Position;
};

struct PSOut
{
    float4 Color       : SV_Target0;
};

PSOut main(PSIn Input)
{
    PSOut Output;

    Output.Color = float4(1.0, 0.0, 0.0, 1.0);

    return Output;
}