
cbuffer Root : register(b0)
{
    uint indirect_box_buffer_count;
}

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

//Buffer with the offsets of all the culled boxes, the first element is the number of boxes in the second pass
StructuredBuffer<uint> second_pass_indirect_culled_boxes : register(t3);

//Indirect parameters for draw call
RWStructuredBuffer<uint> indirect_culled_boxes_parameters : register(u0);

//Buffer with the offsets of all the culled boxes
RWStructuredBuffer<uint> indirect_culled_boxes : register(u1);

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
void second_pass_clear_indirect_arguments()
{
    indirect_culled_boxes_parameters[0] = 18 * 16; //18 indexes per cube, 16 cubes per instance
    indirect_culled_boxes_parameters[1] = 1;
    indirect_culled_boxes_parameters[2] = 0;
    indirect_culled_boxes_parameters[3] = 0;
    indirect_culled_boxes_parameters[4] = 0;

    indirect_culled_boxes[0] = 1;
}

//Second pass box culling, each lane will be a box in the second pass, in groups of 32
[numthreads(32, 1, 1)]
void second_pass_box_culling(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    //Number of boxes
    uint num_second_pass_boxes = second_pass_indirect_culled_boxes[0] - 1;

    if (dispatch_thread_id.x < num_second_pass_boxes)
    {
        //Read the box and do the culling with the HiZ

        uint indirect_box = second_pass_indirect_culled_boxes[dispatch_thread_id.x + 1];

        //Extract the instance offset and the box index
        uint instance_data_offset_byte = (indirect_box >> 8) * 16;
        uint box_index = indirect_box & 0xFF;

        //Read the instance data
        float4 instance_data[3];

        instance_data[0] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 0));
        instance_data[2] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 32));

        float4 instance_rotate_quat = instance_data[2];
        float3 instance_translation = float3(instance_data[0].x, instance_data[0].y, instance_data[0].z);
        uint box_list_offset_byte = asuint(instance_data[0].w);
        box_list_offset_byte += 16; //Skip the box_list count
        box_list_offset_byte += box_index * 16 * 3; //Each box is a float4 * 3

        //Read the box data
        float4 box_data[2];
        box_data[0] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 0));
        box_data[1] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 16));

        float3 box_extent = float3(box_data[1].x, box_data[1].y, box_data[1].z);
        float3 box_translation = float3(box_data[0].x, box_data[0].y, box_data[0].z);
  
        {
            //Calculate 8 points in the box in the current frame
            float3 box_points[8];
            box_points[0] = QuatMultiplication(instance_rotate_quat, float3(-1.f, -1.f, -1.f) * box_extent + box_translation) + instance_translation;
            box_points[1] = QuatMultiplication(instance_rotate_quat, float3(-1.f, -1.f, 1.f) * box_extent + box_translation) + instance_translation;
            box_points[2] = QuatMultiplication(instance_rotate_quat, float3(-1.f, 1.f, -1.f) * box_extent + box_translation) + instance_translation;
            box_points[3] = QuatMultiplication(instance_rotate_quat, float3(-1.f, 1.f, 1.f) * box_extent + box_translation) + instance_translation;
            box_points[4] = QuatMultiplication(instance_rotate_quat, float3(1.f, -1.f, -1.f) * box_extent + box_translation) + instance_translation;
            box_points[5] = QuatMultiplication(instance_rotate_quat, float3(1.f, -1.f, 1.f) * box_extent + box_translation) + instance_translation;
            box_points[6] = QuatMultiplication(instance_rotate_quat, float3(1.f, 1.f, -1.f) * box_extent + box_translation) + instance_translation;
            box_points[7] = QuatMultiplication(instance_rotate_quat, float3(1.f, 1.f, 1.f) * box_extent + box_translation) + instance_translation;

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
            uint lod_index = uint(ceil(log2(max_distance)));

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
            hiz_sample[3] = HiZ[mip_info.xy + uint2(max_box_hiz.x >> lod_index, max_box_hiz.y >> lod_index)];

            float min_hiz = min(hiz_sample[0], min(hiz_sample[1], min(hiz_sample[2], hiz_sample[3])));

            if (max_box_z < min_hiz)
            {
                //Occluded, nothing to do
                return;
            }
            else
            {
                //It is visible, added to the indirect box for the second pass render
                uint offset;
                InterlockedAdd(indirect_culled_boxes[0], 1, offset);

                //Check if we need a new instance group
                if ((offset - 1) / 16 != (offset) / 16)
                {
                    uint offset_instance;
                    InterlockedAdd(indirect_culled_boxes_parameters[1], 1, offset_instance);
                }

                if (offset < indirect_box_buffer_count)
                {
                    indirect_culled_boxes[offset] = indirect_box;
                }
            }
        }
    }
};
