#if defined(EIDOLON_D3D11)
#define EIDOLON_REGISTER(binding, space_index) register(binding)
#else
#define EIDOLON_REGISTER(binding, space_index) register(binding, space_index)
#endif

Texture2D base_color_texture : EIDOLON_REGISTER(t0, space2);
SamplerState base_color_sampler : EIDOLON_REGISTER(s0, space2);

struct PixelInput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

float4 main(PixelInput input) : SV_Target0
{
    float4 color = base_color_texture.Sample(base_color_sampler, input.texcoord);
    clip(color.a - 0.05);
    return color;
}
