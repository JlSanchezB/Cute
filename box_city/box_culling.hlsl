
cbuffer Root : register(b0)
{
    uint instance_lists_offset;
    uint indirect_box_buffer_count;
}

cbuffer ViewData : register(b1)
{
    float4x4 view_projection_matrix;
    float4 time;
    float4 sun_direction;
    float4 frustum_planes[6];
    float4 frustum_points[8];
};

ByteAddressBuffer static_gpu_memory: register(t0);
ByteAddressBuffer dynamic_gpu_memory: register(t1);

//Indirect parameters for draw call
RWStructuredBuffer<uint> indirect_culled_boxes_parameters : register(u0);

//Buffer with the offsets of all the culled boxes
RWStructuredBuffer<uint> indirect_culled_boxes : register(u1);

float3 quat_multiplication(float4 quat, float3 vector)
{
    float3 qv = float3(quat.x, quat.y, quat.z);
    float s = quat.w;
    float3 t = 2.0f * cross(qv, vector);
    return vector + (s * t) + cross(qv, t);
}

//Clear
[numthreads(1, 1, 1)]
void clear_indirect_arguments()
{
    indirect_culled_boxes_parameters[0] = 36;
    indirect_culled_boxes_parameters[1] = 0;
    indirect_culled_boxes_parameters[2] = 0;
    indirect_culled_boxes_parameters[3] = 0;
    indirect_culled_boxes_parameters[4] = 0;
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
            float4 instance_data[3];

            instance_data[0] = asfloat(static_gpu_memory.Load4(instance_offset + 0));
            instance_data[1] = asfloat(static_gpu_memory.Load4(instance_offset + 16));
            instance_data[2] = asfloat(static_gpu_memory.Load4(instance_offset + 32));

            float4 rotate_quat = instance_data[2];
            float3 extent = float3(instance_data[1].x, instance_data[1].y, instance_data[1].z);
            float3 translation = float3(instance_data[0].x, instance_data[0].y, instance_data[0].z);

            //Frustum collision
            {
                float3 box_points[8];
                box_points[0] = quat_multiplication(rotate_quat, float3(-1.f, -1.f, -1.f) * extent) + translation;
                box_points[1] = quat_multiplication(rotate_quat, float3(-1.f, -1.f, 1.f) * extent) + translation;
                box_points[2] = quat_multiplication(rotate_quat, float3(-1.f, 1.f, -1.f) * extent) + translation;
                box_points[3] = quat_multiplication(rotate_quat, float3(-1.f, 1.f, 1.f) * extent) + translation;
                box_points[4] = quat_multiplication(rotate_quat, float3(1.f, -1.f, -1.f) * extent) + translation;
                box_points[5] = quat_multiplication(rotate_quat, float3(1.f, -1.f, 1.f) * extent) + translation;
                box_points[6] = quat_multiplication(rotate_quat, float3(1.f, 1.f, -1.f) * extent) + translation;
                box_points[7] = quat_multiplication(rotate_quat, float3(1.f, 1.f, 1.f) * extent) + translation;

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
                        continue;
                    }
                }
                float3 box_max = box_points[0];
                float3 box_min = box_points[0];
                for (int j = 1; j < 8; ++j)
                {
                    box_min = min(box_min, box_points[j]);
                    box_max = max(box_max, box_points[j]);
                }

                // check frustum outside/inside box
                int outside;
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].x > box_max.x) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].x < box_min.x) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].y > box_max.y) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].y < box_min.y) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].z > box_max.z) ? 1 : 0); if (outside == 8) continue;};
                {outside = 0; for (int k = 0; k < 8; k++) outside += ((frustum_points[k].z < box_min.z) ? 1 : 0); if (outside == 8) continue;};
            }
            
            //Read the GPUBoxList number of boxes
            uint box_list_offset = asuint(instance_data[0].w);

            if (box_list_offset != 0)
            {
                uint box_list_count = static_gpu_memory.Load(box_list_offset);

                //Reserve space in the indirect box buffer
                uint offset;
                InterlockedAdd(indirect_culled_boxes_parameters[1], box_list_count, offset);

                //Loop for each box, REALLY BAD for performance
                for (uint j = 0; j < box_list_count; ++j)
                {
                    uint indirect_box; //Encode the instance offset and the index of the box
                    indirect_box = ((instance_offset / 16) << 8) | j;
                    
                    //Add into the indirect buffer
                    if ((offset + j) < indirect_box_buffer_count)
                    {
                        indirect_culled_boxes[offset + j] = indirect_box;
                    }
                }
                
            }
        }
    }
};
