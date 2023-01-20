struct GameConstant
{
    float4 time;
};

Texture2D texture_test : t0;
SamplerState static_sampler : s1;
GameConstant game_constant : c0;

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
    float2 wave_tex = input.tex + 0.01f * float2(cos(10.f * (game_constant.time.x + input.tex.x)), sin(10.f * (game_constant.time.x + input.tex.y)));
    return texture_test.Sample(static_sampler, wave_tex);
}