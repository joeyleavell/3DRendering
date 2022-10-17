/*cbuffer VertexData : register(b0, space0)
{
    float4x4 ViewProjection;
}*/

struct VSIn
{
    float3 Position : SV_Position;
};

struct VSOut
{
    float4 Position : SV_Position;
};

VSOut main(VSIn Input)
{
    VSOut Output;

    Output.Position = float4(Input.Position, 1.0);

    return Output;
}