cbuffer VertexData : register(b0, space0)
{
    float4x4 ViewProjection;
}

struct VSIn
{
    float3 Position : SV_Position;
    float3 Normal : NORMAL0;
};

struct VSOut
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL0;
};

VSOut main(VSIn Input)
{
    VSOut Output;

    Output.Position = ViewProjection * float4(Input.Position.x, Input.Position.z, Input.Position.y, 1.0);
    Output.Normal = Input.Normal;

    return Output;
}