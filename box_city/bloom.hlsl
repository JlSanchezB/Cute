
#include "constants.hlsl"

RWTexture2D<float4> destination : u0;
Texture2D source : t0;
Texture2D source_blur : t0;
SamplerState source_sampler : s0;

[numthreads(8, 8, 1)]
void down_sample(uint3 thread_id : SV_DispatchThreadID)
{
    uint dest_width, dest_height;
	destination.GetDimensions(dest_width, dest_height);
    
    if (all(thread_id.xy < uint2(dest_width, dest_height)))
    {
        float2 uv = (float2(thread_id.xy) + 0.5f) / float2(dest_width, dest_height);

        float3 colour = float3( 0.0f, 0.0f ,0.0f );
    
        uint width, height;
        source.GetDimensions(width, height);
        float2 inv_pixel_size = float2(1.f / width, 1.f / height);

        // 4 central samples
        float2 center_uv_1 = uv + inv_pixel_size * float2(-1.0f, 1.0f);
        float2 center_uv_2 = uv + inv_pixel_size * float2( 1.0f, 1.0f);
        float2 center_uv_3 = uv + inv_pixel_size * float2(-1.0f,-1.0f);
        float2 center_uv_4 = uv + inv_pixel_size * float2( 1.0f,-1.0f);

        colour += source.Sample(source_sampler, center_uv_1).rgb;
        colour += source.Sample(source_sampler, center_uv_2).rgb;
        colour += source.Sample(source_sampler, center_uv_3).rgb;
        colour += source.Sample(source_sampler, center_uv_4).rgb;

        float3 out_colour = (colour / 4.0f) * 0.5f;

        // 3 row samples
        colour = float3( 0.0f, 0.0f ,0.0f );

        float2 row_uv_1 = uv + inv_pixel_size * float2(-2.0f, 2.0f);
        float2 row_uv_2 = uv + inv_pixel_size * float2( 0.0f, 2.0f);
        float2 row_uv_3 = uv + inv_pixel_size * float2( 2.0f, 2.0f);

        float2 row_uv_4 = uv + inv_pixel_size * float2(-2.0f, 0.0f);
        float2 row_uv_5 = uv + inv_pixel_size * float2( 0.0f, 0.0f);
        float2 row_uv_6 = uv + inv_pixel_size * float2( 2.0f, 0.0f);

        float2 row_uv_7 = uv + inv_pixel_size * float2(-2.0f,-2.0f);
        float2 row_uv_8 = uv + inv_pixel_size * float2( 0.0f,-2.0f);
        float2 row_uv_9 = uv + inv_pixel_size * float2( 2.0f,-2.0f);

        colour += source.Sample(source_sampler, row_uv_1).rgb;
        colour += source.Sample(source_sampler, row_uv_2).rgb;
        colour += source.Sample(source_sampler, row_uv_3).rgb;

        colour += source.Sample(source_sampler, row_uv_4).rgb;
        colour += source.Sample(source_sampler, row_uv_5).rgb;
        colour += source.Sample(source_sampler, row_uv_6).rgb;

        colour += source.Sample(source_sampler, row_uv_7).rgb;
        colour += source.Sample(source_sampler, row_uv_8).rgb;
        colour += source.Sample(source_sampler, row_uv_9).rgb;

        out_colour += (colour / 9.0f) * 0.5f;

        //Write out
        destination[thread_id.xy] = float4(out_colour, 1.f);
    }
}

[numthreads(8, 8, 1)]
void up_sample(uint3 thread_id : SV_DispatchThreadID)
{
    uint dest_width, dest_height;
	destination.GetDimensions(dest_width, dest_height);
    
    float radius = bloom_radius;

    if (all(thread_id.xy < uint2(dest_width, dest_height)))
    {
        float2 uv = (float2(thread_id.xy) + 0.5f) / float2(dest_width, dest_height);

        float3 colour = float3( 0.0f, 0.0f ,0.0f );
    
        uint width, height;
        source_blur.GetDimensions(width, height);
        float2 inv_pixel_size = float2(1.f / width, 1.f / height);

        // 4 central corner samples
        float2 center_uv_1 = uv + radius * inv_pixel_size * float2(-1.0f, 1.0f);
        float2 center_uv_2 = uv + radius * inv_pixel_size * float2( 1.0f, 1.0f);
        float2 center_uv_3 = uv + radius * inv_pixel_size * float2(-1.0f,-1.0f);
        float2 center_uv_4 = uv + radius * inv_pixel_size * float2(1.0f,-1.0f);

        float weight_corner = 1.f;
        colour += weight_corner * source_blur.Sample(source_sampler, center_uv_1).rgb;
        colour += weight_corner * source_blur.Sample(source_sampler, center_uv_2).rgb;
        colour += weight_corner * source_blur.Sample(source_sampler, center_uv_3).rgb;
        colour += weight_corner * source_blur.Sample(source_sampler, center_uv_4).rgb;

        // 4 central cross samples
        float2 cross_uv_1 = uv + radius * inv_pixel_size * float2(-1.0f, 0.0f);
        float2 cross_uv_2 = uv + radius * inv_pixel_size * float2(1.0f, 0.0f);
        float2 cross_uv_3 = uv + radius * inv_pixel_size * float2(0.0f,-1.0f);
        float2 cross_uv_4 = uv + radius * inv_pixel_size * float2(0.0f,-1.0f);

        float weight_cross = 2.f;
        colour += weight_cross * source_blur.Sample(source_sampler, cross_uv_1).rgb;
        colour += weight_cross * source_blur.Sample(source_sampler, cross_uv_2).rgb;
        colour += weight_cross * source_blur.Sample(source_sampler, cross_uv_3).rgb;
        colour += weight_cross * source_blur.Sample(source_sampler, cross_uv_4).rgb;
        
        //Center, weight 4
        colour += 4.f * source_blur.Sample(source_sampler, uv).rgb;

        colour = (colour / 16.0f);

        //Write out, needs to add the source and then the blur
        destination[thread_id.xy] = source[thread_id.xy] + float4(colour, 1.f);
    }
}

