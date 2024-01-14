
#include "constants.hlsl"

RWTexture2D<float4> result : register(u0);
Texture2D scene_buffer : register(t0);
Texture2D<float> depth_buffer : register(t1);

[numthreads(8, 8, 1)]
void fog(uint3 thread_id : SV_DispatchThreadID)
{
    uint dest_width, dest_height;
	result.GetDimensions(dest_width, dest_height);
    
    if (all(thread_id.xy < uint2(dest_width, dest_height)))
    {
        //Read depth buffer
        float depth_value = depth_buffer[thread_id.xy];

        //Calculate world position
        float2 viewport_position = float2(thread_id.xy) / float2(dest_width, dest_height);
        viewport_position = 2.f * viewport_position - 1.f;
        float4 screen_position = float4(viewport_position.x, viewport_position.y, depth_value, 1.f);

        float4 world_position = mul(view_projection_matrix_inv, screen_position);
        world_position = world_position / world_position.w;

        //Calculate distance to the camera
        float camera_distance = length(world_position.xyz - camera_position.xyz);

        //Calculate fog
        float fog = exp2(-fog_density * camera_distance);

        //Read colour buffer
        float3 scene = scene_buffer[thread_id.xy].xyz;
       
        //Write out
        result[thread_id.xy] = float4(lerp(fog_colour, scene, saturate(fog)), 1.f);
    }
}

