
cbuffer ViewData : register(b1)
{
    float4x4 view_projection_matrix;
    float4x4 last_frame_view_projection_matrix;
    float4 camera_position;
    float4 time;
    float4 sun_direction;
    float4 frustum_planes[6];
    float4 frustum_points[8];
};

ByteAddressBuffer static_gpu_memory: register(t0);
ByteAddressBuffer dynamic_gpu_memory: register(t1);
Texture2D<float> HiZ : register(t2);

uint2 get_hiz_mip_offset(uint mip_index)
{
    if (mip_index == 0)
    {
        return uint2(0, 0);
    }
    return uint2(512, 512 - 1024 / exp2(mip_index));
}

float3 quat_multiplication(float4 quat, float3 vector)
{
    float3 qv = float3(quat.x, quat.y, quat.z);
    float s = -quat.w;
    float3 t = 2.0f * cross(qv, vector);
    return vector + (s * t) + cross(qv, t);
}

//Visible using the frustum planes
bool is_visible(float4 instance_rotate_quad, float3 instance_position, float3 box_extent, float3 box_position)
{
    float3 box_points[8];
    box_points[0] = quat_multiplication(instance_rotate_quad, float3(-1.f, -1.f, -1.f) * box_extent + box_position) + instance_position;
    box_points[1] = quat_multiplication(instance_rotate_quad, float3(-1.f, -1.f, 1.f) * box_extent + box_position) + instance_position;
    box_points[2] = quat_multiplication(instance_rotate_quad, float3(-1.f, 1.f, -1.f) * box_extent + box_position) + instance_position;
    box_points[3] = quat_multiplication(instance_rotate_quad, float3(-1.f, 1.f, 1.f) * box_extent + box_position) + instance_position;
    box_points[4] = quat_multiplication(instance_rotate_quad, float3(1.f, -1.f, -1.f) * box_extent + box_position) + instance_position;
    box_points[5] = quat_multiplication(instance_rotate_quad, float3(1.f, -1.f, 1.f) * box_extent + box_position) + instance_position;
    box_points[6] = quat_multiplication(instance_rotate_quad, float3(1.f, 1.f, -1.f) * box_extent + box_position) + instance_position;
    box_points[7] = quat_multiplication(instance_rotate_quad, float3(1.f, 1.f, 1.f) * box_extent + box_position) + instance_position;


    // check box outside/inside of frustum
    for (int ii = 0; ii < 6; ii++)
    {
        if ((dot(frustum_planes[ii], float4(box_points[0], 1.0f)) < 0.0f) &&
            (dot(frustum_planes[ii], float4(box_points[1], 1.0f)) < 0.0f) &&
            (dot(frustum_planes[ii], float4(box_points[2], 1.0f)) < 0.0f) &&
            (dot(frustum_planes[ii], float4(box_points[3], 1.0f)) < 0.0f) &&
            (dot(frustum_planes[ii], float4(box_points[4], 1.0f)) < 0.0f) &&
            (dot(frustum_planes[ii], float4(box_points[5], 1.0f)) < 0.0f) &&
            (dot(frustum_planes[ii], float4(box_points[6], 1.0f)) < 0.0f) &&
            (dot(frustum_planes[ii], float4(box_points[7], 1.0f)) < 0.0f))
        {
            //Outside
            return false;
        }
    }

    float3 box_max = box_points[0];
    float3 box_min = box_points[0];
    for (int j = 1; j < 8; ++j)
    {
        box_min = min(box_min, box_points[j]);
        box_max = max(box_max, box_points[j]);
    }

    int outside = 0;

    // check frustum outside/inside box
    {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].x > box_max.x) ? 1 : 0); if (outside == 8) return false; };
    {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].x < box_min.x) ? 1 : 0); if (outside == 8) return false; };
    {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].y > box_max.y) ? 1 : 0); if (outside == 8) return false; };
    {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].y < box_min.y) ? 1 : 0); if (outside == 8) return false; };
    {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].z > box_max.z) ? 1 : 0); if (outside == 8) return false; };
    {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].z < box_min.z) ? 1 : 0); if (outside == 8) return false; };

    return true;
}

//Visible using the HiZ
bool hiz_is_visible(float4 instance_rotate_quad, float3 instance_position, float3 box_extent, float3 box_position)
{
    //Calculate 8 points in the box, last frame
    float3 box_points[8];
    box_points[0] = quat_multiplication(instance_rotate_quad, float3(-1.f, -1.f, -1.f) * box_extent + box_position) + instance_position;
    box_points[1] = quat_multiplication(instance_rotate_quad, float3(-1.f, -1.f, 1.f) * box_extent + box_position) + instance_position;
    box_points[2] = quat_multiplication(instance_rotate_quad, float3(-1.f, 1.f, -1.f) * box_extent + box_position) + instance_position;
    box_points[3] = quat_multiplication(instance_rotate_quad, float3(-1.f, 1.f, 1.f) * box_extent + box_position) + instance_position;
    box_points[4] = quat_multiplication(instance_rotate_quad, float3(1.f, -1.f, -1.f) * box_extent + box_position) + instance_position;
    box_points[5] = quat_multiplication(instance_rotate_quad, float3(1.f, -1.f, 1.f) * box_extent + box_position) + instance_position;
    box_points[6] = quat_multiplication(instance_rotate_quad, float3(1.f, 1.f, -1.f) * box_extent + box_position) + instance_position;
    box_points[7] = quat_multiplication(instance_rotate_quad, float3(1.f, 1.f, 1.f) * box_extent + box_position) + instance_position;

    //Convert in screen texel space
    float3 clip_box_points[8];
    [unroll] for (uint point_index = 0; point_index < 8; ++point_index)
    {
        float4 view_box_point = mul(last_frame_view_projection_matrix, float4(box_points[point_index], 1.f));
        clip_box_points[point_index] = view_box_point.xyz / view_box_point.w;
    }

    //Calculate the min/max and min_z
    float2 min_box = clip_box_points[0].xy;
    float2 max_box = clip_box_points[0].xy;
    float max_box_z = clip_box_points[0].z;

    [unroll] for (point_index = 1; point_index < 8; ++point_index)
    {
        min_box = min(min_box, clip_box_points[point_index].xy);
        max_box = max(max_box, clip_box_points[point_index].xy);
        max_box_z = max(max_box_z, clip_box_points[point_index].z);
    }

    //Calculate hiZ space 
    int2 min_box_hiz = (min_box * float2(0.5f, -0.5f) + 0.5f) * 512.f;
    int2 max_box_hiz = (max_box * float2(0.5f, -0.5f) + 0.5f) * 512.f;

    //Calculate mip index
    int max_distance = max(max_box_hiz.x - min_box_hiz.x, min_box_hiz.y - max_box_hiz.y);
    uint lod_index = 0;
    if (max_distance > 0)
        lod_index = uint(ceil(log2(max_distance)));

    //Sample the HiZ to decide if can be render in the first pass or it needs to be pass to the second pass
    uint2 mip_info = get_hiz_mip_offset(lod_index);

    //Clamp
    min_box_hiz = max(0, min_box_hiz);
    min_box_hiz = min(511, min_box_hiz);
    max_box_hiz = max(0, max_box_hiz);
    max_box_hiz = min(511, max_box_hiz);

    float hiz_sample[4];
    hiz_sample[0] = HiZ[mip_info.xy + uint2(min_box_hiz.x >> lod_index, min_box_hiz.y >> lod_index)];
    hiz_sample[1] = HiZ[mip_info.xy + uint2(min_box_hiz.x >> lod_index, max_box_hiz.y >> lod_index)];
    hiz_sample[2] = HiZ[mip_info.xy + uint2(max_box_hiz.x >> lod_index, min_box_hiz.y >> lod_index)];
    hiz_sample[3] = HiZ[mip_info.xy + uint2(max_box_hiz.x >> lod_index, max_box_hiz.y >> lod_index)];

    float min_hiz = min(hiz_sample[0], min(hiz_sample[1], min(hiz_sample[2], hiz_sample[3])));

    return max_box_z >= min_hiz;
}