Texture2D base_color_texture : register(t0, space2);
SamplerState base_color_sampler : register(s0, space2);

float4 main(float2 texcoord : TEXCOORD0) : SV_Target0
{
    float4 color = base_color_texture.Sample(base_color_sampler, texcoord);
    clip(color.a - 0.05);
    return color;
}
