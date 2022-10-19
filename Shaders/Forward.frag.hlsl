struct PSIn
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL0;
};

struct PSOut
{
    float4 Color       : SV_Target0;
};

PSOut main(PSIn Input)
{
    PSOut Output;

    float3 NormClamped = (Input.Normal + 1.0f) * 0.5f;
    Output.Color = float4(NormClamped, 1.0f);

    return Output;
}