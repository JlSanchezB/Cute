
#include "constants.hlsl"

ConstantBuffer<ViewDataStruct> ViewData : register(b0);
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
        float4 screen_position = float4(viewport_position.x, -viewport_position.y, depth_value, 1.f);

        float4 world_position = mul(ViewData.view_projection_matrix_inv, screen_position);
        world_position = world_position / world_position.w;

        //View vector
        float3 view_vector = -(world_position.xyz - ViewData.camera_position.xyz);

        //Camera distance
        float camera_distance = length(world_position.xyz - ViewData.camera_position.xyz);

        ///Height fog
        float4 fog_plane = float4(0.f, 0.f, 1.f, -ViewData.fog_top_height);
        float3 av = (ViewData.fog_bottom_height * ViewData.fog_density * 0.66f * 0.00001f / 2.f) * view_vector;
        float k = (ViewData.camera_position.z <= ViewData.fog_top_height) ? 1.f : 0.f;
        float c1 = k * (dot(fog_plane, world_position) + dot(fog_plane, float4(ViewData.camera_position.xyz, 1.f)));
        float c2 = (1.f - 2.f * k) * dot(fog_plane, world_position);
        float f_dot_v = dot(fog_plane, float4(view_vector, 0.f));

        float g = min(c2, 0.f);
        g = -length(av) * (c1 - g * g / abs(f_dot_v));

        float fog = exp(-g);

        //Calculate distance to the camera
        //float camera_distance = length(world_position.xyz - ViewData.camera_position.xyz);

        //Calculate fog
        //float fog = exp2(-ViewData.fog_density * camera_distance);

        //Read colour buffer
        float3 scene = scene_buffer[thread_id.xy].xyz;
       
        //Write out
        result[thread_id.xy] = float4(lerp(ViewData.fog_colour, scene, saturate(fog)), 1.f);
    }
}

