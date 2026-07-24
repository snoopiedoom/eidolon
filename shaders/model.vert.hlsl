struct VertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    uint4 joints : BLENDINDICES0;
    float4 weights : BLENDWEIGHT0;
    float3 neutral_delta : POSITION1;
    float3 focused_delta : POSITION2;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

#if defined(EIDOLON_D3D11)
#define EIDOLON_REGISTER(binding, space_index) register(binding)
#else
#define EIDOLON_REGISTER(binding, space_index) register(binding, space_index)
#endif

cbuffer Scene : EIDOLON_REGISTER(b0, space1)
{
    row_major float4x4 model_view_projection;
    float4 expression_weights;
};

cbuffer Bones : EIDOLON_REGISTER(b1, space1)
{
    row_major float4x4 joint_matrices[256];
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    row_major float4x4 skin =
        joint_matrices[input.joints.x] * input.weights.x +
        joint_matrices[input.joints.y] * input.weights.y +
        joint_matrices[input.joints.z] * input.weights.z +
        joint_matrices[input.joints.w] * input.weights.w;
    float3 morphed_position =
        input.position +
        input.neutral_delta * expression_weights.x +
        input.focused_delta * expression_weights.y;
    float4 skinned_position = mul(float4(morphed_position, 1.0), skin);
    output.position = mul(skinned_position, model_view_projection);
    output.texcoord = input.texcoord;
    return output;
}
