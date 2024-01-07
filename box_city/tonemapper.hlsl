
#include "constants.hlsl"

Texture2D light_scene : t0;
SamplerState light_scene_sampler : s0;

struct PSInput
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

PSInput vs_main(float4 position : POSITION, float2 tex : TEXCOORD0)
{
    PSInput result;

    result.position = position;
    result.tex = tex;

    return result;
}

float4 ps_main(PSInput input) : SV_TARGET
{
    float3 colour = light_scene.Sample(light_scene_sampler, input.tex).rgb * exposure.x;
    return float4(colour / (1.f + colour), 1.f);
}