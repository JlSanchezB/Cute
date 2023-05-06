
cbuffer Root : register(b0)
{
    uint instance_lists_offset;
    uint indirect_box_buffer_count;
}

cbuffer ViewData : register(b1)
{
    float4x4 view_projection_matrix;
    float4 camera_position;
    float4 time;
    float4 sun_direction;
    float4 frustum_planes[6];
    float4 frustum_points[8];
};

ByteAddressBuffer static_gpu_memory: register(t0);
ByteAddressBuffer dynamic_gpu_memory: register(t1);
Texture2D<float> HiZ : register(t2);

//Indirect parameters for draw call
RWStructuredBuffer<uint> indirect_culled_boxes_parameters : register(u0);

//Buffer with the offsets of all the culled boxes
RWStructuredBuffer<uint> indirect_culled_boxes : register(u1);

//Indirect parameters for second pass culling, it is the group count
RWStructuredBuffer<uint> second_pass_indirect_culled_boxes_parameters : register(u2);

//Buffer with the offsets of all the culled boxes, the first element is the number of boxes in the second pass
RWStructuredBuffer<uint> second_pass_indirect_culled_boxes : register(u3);

uint2 GetMipOffset(uint mip_index)
{
    if (mip_index == 0)
    {
        return uint2(0, 0);
    }
    return uint2(512, 512 - 1024 / exp2(mip_index));
}

float3 QuatMultiplication(float4 quat, float3 vector)
{
    float3 qv = float3(quat.x, quat.y, quat.z);
    float s = -quat.w;
    float3 t = 2.0f * cross(qv, vector);
    return vector + (s * t) + cross(qv, t);
}

//Clear
[numthreads(1, 1, 1)]
void clear_indirect_arguments()
{
    indirect_culled_boxes_parameters[0] = 18;
    indirect_culled_boxes_parameters[1] = 0;
    indirect_culled_boxes_parameters[2] = 0;
    indirect_culled_boxes_parameters[3] = 0;
    indirect_culled_boxes_parameters[4] = 0;

    second_pass_indirect_culled_boxes_parameters[0] = 0;
    second_pass_indirect_culled_boxes_parameters[1] = 1;
    second_pass_indirect_culled_boxes_parameters[2] = 1;

    //Reset the counter, the first slot is the counter
    second_pass_indirect_culled_boxes[0] = 1;
}

//Naive implementation for culling
//Each group will get one instance list
[numthreads(32, 1, 1)]
void box_culling(uint3 group : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID)
{
    //Each group handles one instance list
    uint instance_list_offset = dynamic_gpu_memory.Load(instance_lists_offset + (1 + group.x) * 4);

    //Read number of instances
    uint num_instances = static_gpu_memory.Load(instance_list_offset);

    instance_list_offset += 4; //The num is read, now are the offsets

    //Number of passes needed to process all the instances
    uint num_passes = 1 + (num_instances - 1) / 32;

    //For each pass
    for (uint i = 0; i < num_passes; ++i)
    {
        //Each thread will cull one box

        uint instance_index = i * 32 + group_thread_id.x;

        if (instance_index < num_instances)
        {
            //Calculate the offset with the instance data
            uint instance_offset = static_gpu_memory.Load(instance_list_offset + instance_index * 4);

            //Read Box instance data
            float4 instance_data[5];

            instance_data[0] = asfloat(static_gpu_memory.Load4(instance_offset + 0));
            instance_data[1] = asfloat(static_gpu_memory.Load4(instance_offset + 16));
            instance_data[2] = asfloat(static_gpu_memory.Load4(instance_offset + 32));
            instance_data[3] = asfloat(static_gpu_memory.Load4(instance_offset + 48));
            instance_data[4] = asfloat(static_gpu_memory.Load4(instance_offset + 64));

            float4 rotate_quat = instance_data[2];
            float3 extent = float3(instance_data[1].x, instance_data[1].y, instance_data[1].z);
            float3 translation = float3(instance_data[0].x, instance_data[0].y, instance_data[0].z);
            float3 last_frame_translation = float3(instance_data[3].x, instance_data[3].y, instance_data[3].z);
            float4 last_frame_rotate_quat = instance_data[4];
            uint box_list_offset_byte = asuint(instance_data[0].w);

            //Frustum collision
            {
                float3 box_points[8];
                box_points[0] = QuatMultiplication(rotate_quat, float3(-1.f, -1.f, -1.f) * extent) + translation;
                box_points[1] = QuatMultiplication(rotate_quat, float3(-1.f, -1.f, 1.f) * extent) + translation;
                box_points[2] = QuatMultiplication(rotate_quat, float3(-1.f, 1.f, -1.f) * extent) + translation;
                box_points[3] = QuatMultiplication(rotate_quat, float3(-1.f, 1.f, 1.f) * extent) + translation;
                box_points[4] = QuatMultiplication(rotate_quat, float3(1.f, -1.f, -1.f) * extent) + translation;
                box_points[5] = QuatMultiplication(rotate_quat, float3(1.f, -1.f, 1.f) * extent) + translation;
                box_points[6] = QuatMultiplication(rotate_quat, float3(1.f, 1.f, -1.f) * extent) + translation;
                box_points[7] = QuatMultiplication(rotate_quat, float3(1.f, 1.f, 1.f) * extent) + translation;

                int outside = 0;
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
                        outside = 1;
                        continue;
                    }
                }
                if (outside)
                    continue;

                float3 box_max = box_points[0];
                float3 box_min = box_points[0];
                for (int j = 1; j < 8; ++j)
                {
                    box_min = min(box_min, box_points[j]);
                    box_max = max(box_max, box_points[j]);
                }

                // check frustum outside/inside box
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].x > box_max.x) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].x < box_min.x) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].y > box_max.y) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].y < box_min.y) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].z > box_max.z) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].z < box_min.z) ? 1 : 0); if (outside == 8) continue;};
            }

            //TODO, implement small instance culling

            
            //Read the GPUBoxList number of boxes
            uint box_list_offset = asuint(instance_data[0].w);

            if (box_list_offset != 0)
            {
                uint box_list_count = static_gpu_memory.Load(box_list_offset);

                //Loop for each box, REALLY BAD for performance
                for (uint j = 0; j < box_list_count; ++j)
                {
                    //Check the Hiz to decide if it needs to be render in the first pass or in the second pass
                    bool first_pass = true;

                    uint box_offset_byte = box_list_offset_byte;
                    box_offset_byte += 16; //Skip the box_list count
                    box_offset_byte += j * 16 * 3; //Each box is a float4 * 3

                    //Read Box data
                    float4 box_data[2];
                    box_data[0] = asfloat(static_gpu_memory.Load4(box_offset_byte + 0));
                    box_data[1] = asfloat(static_gpu_memory.Load4(box_offset_byte + 16));

                    float3 box_extent = float3(box_data[1].x, box_data[1].y, box_data[1].z);
                    float3 box_translation = float3(box_data[0].x, box_data[0].y, box_data[0].z);

                    {
                        //Calculate 8 points in the box, last frame
                        float3 box_points[8];
                        box_points[0] = QuatMultiplication(last_frame_rotate_quat, float3(-1.f, -1.f, -1.f) * box_extent + box_translation) + last_frame_translation;
                        box_points[1] = QuatMultiplication(last_frame_rotate_quat, float3(-1.f, -1.f, 1.f) * box_extent + box_translation) + last_frame_translation;
                        box_points[2] = QuatMultiplication(last_frame_rotate_quat, float3(-1.f, 1.f, -1.f) * box_extent + box_translation) + last_frame_translation;
                        box_points[3] = QuatMultiplication(last_frame_rotate_quat, float3(-1.f, 1.f, 1.f) * box_extent + box_translation) + last_frame_translation;
                        box_points[4] = QuatMultiplication(last_frame_rotate_quat, float3(1.f, -1.f, -1.f) * box_extent + box_translation) + last_frame_translation;
                        box_points[5] = QuatMultiplication(last_frame_rotate_quat, float3(1.f, -1.f, 1.f) * box_extent + box_translation) + last_frame_translation;
                        box_points[6] = QuatMultiplication(last_frame_rotate_quat, float3(1.f, 1.f, -1.f) * box_extent + box_translation) + last_frame_translation;
                        box_points[7] = QuatMultiplication(last_frame_rotate_quat, float3(1.f, 1.f, 1.f) * box_extent + box_translation) + last_frame_translation;

                        //Convert in screen texel space
                        float4 clip_box_points[8];
                        [unroll] for (uint point_index = 0; point_index < 8; ++point_index)
                        {
                            clip_box_points[point_index] = mul(view_projection_matrix, float4(box_points[point_index], 1.f));
                            clip_box_points[point_index] = clip_box_points[point_index] / clip_box_points[point_index].w;
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

                        if (max_distance > 1)
                        {
                            lod_index = uint(ceil(log2(max_distance)));
                        }
                        //Sample the HiZ to decide if can be render in the first pass or it needs to be pass to the second pass
                        uint2 mip_info = GetMipOffset(lod_index);

                        //Clamp
                        min_box_hiz = max(0, min_box_hiz);
                        min_box_hiz = min(511, min_box_hiz);
                        max_box_hiz = max(0, max_box_hiz);
                        max_box_hiz = min(511, max_box_hiz);

                        float hiz_sample[4];
                        hiz_sample[0] = HiZ[mip_info.xy + uint2(min_box_hiz.x >> lod_index, min_box_hiz.y >> lod_index)];
                        hiz_sample[1] = HiZ[mip_info.xy + uint2(min_box_hiz.x >> lod_index, max_box_hiz.y >> lod_index)];
                        hiz_sample[2] = HiZ[mip_info.xy + uint2(max_box_hiz.x >> lod_index, min_box_hiz.y >> lod_index)];
                        hiz_sample[3] = HiZ[mip_info.xy + uint2(max_box_hiz.x >> lod_index, max_box_hiz.y >> lod_index) ];

                        float min_hiz = min(hiz_sample[0], min(hiz_sample[1], min(hiz_sample[2], hiz_sample[3])));

                        if (max_box_z < min_hiz)
                        {
                            //Occluded, it needs to go to the second pass
                            first_pass = false;
                        }
                        else
                        {
                            first_pass = true;
                        }
                    }
                    
                    uint indirect_box; //Encode the instance offset and the index of the box
                    indirect_box = ((instance_offset / 16) << 8) | j;

                    if (first_pass)
                    {
                        //First pass
                        uint offset;
                        InterlockedAdd(indirect_culled_boxes_parameters[1], 1, offset);
                        
                        if (offset < indirect_box_buffer_count)
                        {
                            indirect_culled_boxes[offset] = indirect_box;
                        }

                    }
                    else
                    {
                        //Second pass
                        uint offset;
                        InterlockedAdd(second_pass_indirect_culled_boxes[0], 1, offset);
           
                        if (offset < indirect_box_buffer_count)
                        {
                            second_pass_indirect_culled_boxes[offset] = indirect_box;
                        }

                        //Check if we need a new group
                        if (offset / 32 != (offset + 1) / 32)
                        {
                            //That means we need a new group
                            InterlockedAdd(second_pass_indirect_culled_boxes_parameters[0], 1, offset);
                        }
                    }
                }               
            }
        }
    }
};
