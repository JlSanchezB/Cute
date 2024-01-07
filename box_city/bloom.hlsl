
#include "constants.hlsl"

Texture2D source : t0;
SamplerState source_sampler : s0;

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

float4 downsampler_ps_main(PSInput input) : SV_TARGET
{
    float3 colour = float3( 0.0f, 0.0f ,0.0f );
    
    uint width, height;
    source.GetDimensions(width, height);
    float2 inv_pixel_size = float2(1.f / width, 1.f / height);

    // 4 central samples
    float2 center_uv_1 = input.tex + inv_pixel_size * float2(-1.0f, 1.0f);
    float2 center_uv_2 = input.tex + inv_pixel_size * float2( 1.0f, 1.0f);
    float2 center_uv_3 = input.tex + inv_pixel_size * float2(-1.0f,-1.0f);
    float2 center_uv_4 = input.tex + inv_pixel_size * float2( 1.0f,-1.0f);

    colour += source.Sample(source_sampler, center_uv_1).rgb;
    colour += source.Sample(source_sampler, center_uv_2).rgb;
    colour += source.Sample(source_sampler, center_uv_3).rgb;
    colour += source.Sample(source_sampler, center_uv_4).rgb;

    float3 out_colour = (colour / 4.0f) * 0.5f;

    // 3 row samples
    colour = float3( 0.0f, 0.0f ,0.0f );

    float2 row_uv_1 = input.tex + inv_pixel_size * float2(-2.0f, 2.0f);
    float2 row_uv_2 = input.tex + inv_pixel_size * float2( 0.0f, 2.0f);
    float2 row_uv_3 = input.tex + inv_pixel_size * float2( 2.0f, 2.0f);

    float2 row_uv_4 = input.tex + inv_pixel_size * float2(-2.0f, 0.0f);
    float2 row_uv_5 = input.tex + inv_pixel_size * float2( 0.0f, 0.0f);
    float2 row_uv_6 = input.tex + inv_pixel_size * float2( 2.0f, 0.0f);

    float2 row_uv_7 = input.tex + inv_pixel_size * float2(-2.0f,-2.0f);
    float2 row_uv_8 = input.tex + inv_pixel_size * float2( 0.0f,-2.0f);
    float2 row_uv_9 = input.tex + inv_pixel_size * float2( 2.0f,-2.0f);

    colour += source.Sample(source_sampler, row_uv_1).rgb;
    colour += source.Sample(source_sampler, row_uv_2).rgb;
    colour += source.Sample(source_sampler, row_uv_3).rgb;

    colour += source.Sample(source_sampler, row_uv_4).rgb;
    colour += source.Sample(source_sampler, row_uv_5).rgb;;
    colour += source.Sample(source_sampler, row_uv_6).rgb;

    colour += source.Sample(source_sampler, row_uv_7).rgb;
    colour += source.Sample(source_sampler, row_uv_8).rgb;
    colour += source.Sample(source_sampler, row_uv_9).rgb;

    out_colour += (colour / 9.0f) * 0.5f;

    return float4(out_colour, 1.f);
}

float4 bloom_threshold_ps_main(PSInput input) : SV_TARGET
{

    float3 colour = source.Sample(source_sampler, input.tex).rgb * exposure_bloomthreshold_bloomintensity.x;

    //Calculate luminance
    float luminance = dot(colour, colour);

    if (luminance < exposure_bloomthreshold_bloomintensity.y)
    {
        return float4(0.f, 0.f, 0.f, 1.f);
    }
    else
    {
        return float4(colour / (1.f + colour), 1.f);
    }
}

float4 kawase_blur_downsample_ps_main(PSInput input) : SV_TARGET
{
    uint width, height;
    source.GetDimensions(width, height);
    
    float2 half_pixel = (1.0f / float2(width, height));

    float2 dir_diag1 = float2( -half_pixel.x,  half_pixel.y ); // Top left
    float2 dir_diag2 = float2(  half_pixel.x,  half_pixel.y ); // Top right
    float2 dir_diag3 = float2(  half_pixel.x, -half_pixel.y ); // Bottom right
    float2 dir_diag4 = float2( -half_pixel.x, -half_pixel.y ); // Bottom left

    float3 color = source.Sample(source_sampler, input.tex).rgb * 4.0f;
    color += source.Sample(source_sampler, input.tex + dir_diag1).rgb;
    color += source.Sample(source_sampler, input.tex + dir_diag2).rgb;
    color += source.Sample(source_sampler, input.tex + dir_diag3).rgb;
    color += source.Sample(source_sampler, input.tex + dir_diag4).rgb;

    return float4(color / 8.f, 1.f);
}

float4 kawase_blur_upsample_ps_main(PSInput input) : SV_TARGET
{
    uint width, height;
    source.GetDimensions(width, height);
    
    float2 half_pixel = (1.0f / float2(width, height)) * 0.5f;

    float2 dir_diag1 = float2( -half_pixel.x,  half_pixel.y ); // Top left
    float2 dir_diag2 = float2(  half_pixel.x,  half_pixel.y ); // Top right
    float2 dir_diag3 = float2(  half_pixel.x, -half_pixel.y ); // Bottom right
    float2 dir_diag4 = float2( -half_pixel.x, -half_pixel.y ); // Bottom left
    float2 dir_axis1 = float2( -half_pixel.x,  0.0f );        // Left
    float2 dir_axis2 = float2(  half_pixel.x,  0.0f );        // Right
    float2 dir_axis3 = float2( 0.0f,  half_pixel.y );         // Top
    float2 dir_axis4 = float2( 0.0f, -half_pixel.y );         // Bottom

    float3 color = float3(0.f, 0.f, 0.f);
    color += source.Sample(source_sampler, input.tex + dir_diag1).rgb;
    color += source.Sample(source_sampler, input.tex + dir_diag2).rgb;
    color += source.Sample(source_sampler, input.tex + dir_diag3).rgb;
    color += source.Sample(source_sampler, input.tex + dir_diag4).rgb;

    color += source.Sample(source_sampler, input.tex + dir_axis1).rgb * 2.f;
    color += source.Sample(source_sampler, input.tex + dir_axis2).rgb * 2.f;
    color += source.Sample(source_sampler, input.tex + dir_axis3).rgb * 2.f;
    color += source.Sample(source_sampler, input.tex + dir_axis4).rgb * 2.f;

    return float4(color / 12.f, 1.f);
}