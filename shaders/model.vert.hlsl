struct VertexInput
{
    float3 position : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

cbuffer Scene : register(b0, space1)
{
    row_major float4x4 model_view_projection;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), model_view_projection);
    output.texcoord = input.texcoord;
    return output;
}
