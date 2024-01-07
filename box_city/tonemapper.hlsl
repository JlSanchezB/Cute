
#include "constants.hlsl"

Texture2D light_scene : t0;
SamplerState light_scene_sampler : s0;
Texture2D light_scene_4 : t1;

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
    float3 colour = light_scene.Sample(light_scene_sampler, input.tex).rgb * exposure_bloomthreshold_bloomintensity.x;
    
    //Tonemapper
    colour = colour / (1.f + colour);

    //Add Bloom
    float3 bloom = light_scene_4.Sample(light_scene_sampler, input.tex).rgb;

    return float4(colour + bloom * exposure_bloomthreshold_bloomintensity.z, 1.f);
}