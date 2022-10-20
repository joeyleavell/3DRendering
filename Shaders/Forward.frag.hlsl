struct DirectionalLight
{
    float3 mDir;
};

cbuffer FragmentData : register(b1, space0)
{
    float3 mEye;
	DirectionalLight mDirLight;
}

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

    float NdotL = dot(Input.Normal, mDirLight.mDir);
    Output.Color = float4(float3(1.0f, 1.0f, 1.0f) * NdotL, 1.0f);

    return Output;
}