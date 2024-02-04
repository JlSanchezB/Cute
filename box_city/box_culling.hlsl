CONTROL_VARIABLE(bool, BoxCulling, MainPassFrustumCulling, true)
CONTROL_VARIABLE(bool, BoxCulling, SkipMainPassCulling, false)
COUNTER(BoxCulling, TotalInstances)
COUNTER(BoxCulling, TotalCubes)
COUNTER(BoxCulling, FrustumVisibleInstances)
COUNTER(BoxCulling, FirstPassVisibleCubes)

cbuffer Root : register(b0)
{
    uint instance_lists_offset;
    uint indirect_box_buffer_count;
    uint second_pass_indirect_box_buffer_count;
}

//Indirect parameters for draw call
RWStructuredBuffer<uint> indirect_culled_boxes_parameters : register(u0);

//Buffer with the offsets of all the culled boxes
RWStructuredBuffer<uint> indirect_culled_boxes : register(u1);

//Indirect parameters for second pass culling, it is the group count
RWStructuredBuffer<uint> second_pass_indirect_culled_boxes_parameters : register(u2);

//Buffer with the offsets of all the culled boxes, the first element is the number of boxes in the second pass
RWStructuredBuffer<uint> second_pass_indirect_culled_boxes : register(u3);

#include "box_culling_common.hlsl"

//Clear
[numthreads(1, 1, 1)]
void clear_indirect_arguments()
{
    indirect_culled_boxes_parameters[0] = 18 * 16; //18 indexes per cube, 16 cubes per instance
    indirect_culled_boxes_parameters[1] = 1;
    indirect_culled_boxes_parameters[2] = 0;
    indirect_culled_boxes_parameters[3] = 0;
    indirect_culled_boxes_parameters[4] = 0;

    second_pass_indirect_culled_boxes_parameters[0] = 1;
    second_pass_indirect_culled_boxes_parameters[1] = 1;
    second_pass_indirect_culled_boxes_parameters[2] = 1;

    //Reset the counter, the first slot is the counter
    second_pass_indirect_culled_boxes[0] = 1;
    indirect_culled_boxes[0] = 1;
}

//Naive implementation for culling
//Each group will get one instance list
[numthreads(32, 1, 1)]
void box_culling(uint3 group : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID)
{
    uint instance_list_index = group.x;

    //Each group handles one instance list
    uint instance_list_offset = dynamic_gpu_memory.Load(instance_lists_offset + (1 + instance_list_index) * 4);

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
            COUNTER_INC(TotalInstances);

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
            uint box_list_offset_byte = asuint(instance_data[1].w);

            //Frustum collision, check if the instance is visible
            if (!is_visible(rotate_quat, translation, extent, float3(0.f, 0.f, 0.f)) && MainPassFrustumCulling)
            {
                continue;
            }

            COUNTER_INC(FrustumVisibleInstances);

            //TODO, implement small instance culling
            
            //Read the GPUBoxList number of boxes
            {
                uint box_list_count = static_gpu_memory.Load(box_list_offset_byte);

                COUNTER_INC_VALUE(TotalCubes, box_list_count);
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
            
                    if (!hiz_is_visible(last_frame_rotate_quat, last_frame_translation, box_extent, box_translation) || SkipMainPassCulling)
                    {
                        //Occluded, it needs to go to the second pass
                        first_pass = false;
                    }
                    else
                    {
                        first_pass = true;
                    }
                    
                    uint indirect_box; //Encode the instance list index (8bits), instance index (12bits), box index (12 bits);
                    assert(instance_list_index < (1 << 8)); //check range
                    assert(instance_index < (1 << 12)); //check range
                    assert(j < (1 << 12)); //check range
                    indirect_box = ((instance_list_index & 0xFF) << 24) | ((instance_index & 0xFFF)  << 12) | (j & 0xFFF);

                    if (first_pass)
                    {
                        //First pass
                        uint offset;
                        InterlockedAdd(indirect_culled_boxes[0], 1, offset);

                        //Check if we need a new instance group, 16 cubes in each instance
                        if ((offset - 1) / 16 != (offset) / 16)
                        {
                            uint offset_instance;
                            InterlockedAdd(indirect_culled_boxes_parameters[1], 1, offset_instance);
                        }

                        assert(offset < indirect_box_buffer_count); //check range
                        if (offset < indirect_box_buffer_count)
                        {
                            indirect_culled_boxes[offset] = indirect_box;
                        }

                        COUNTER_INC(FirstPassVisibleCubes);

                    }
                    else
                    {
                        //Second pass
                        uint offset;
                        InterlockedAdd(second_pass_indirect_culled_boxes[0], 1, offset);
           
                        assert(offset < second_pass_indirect_box_buffer_count); //check range
                        if (offset < second_pass_indirect_box_buffer_count)
                        {
                            second_pass_indirect_culled_boxes[offset] = indirect_box;

                            //Check if we need a new group
                            if ((offset - 1) / 32 != (offset) / 32)
                            {
                                //That means we need a new group
                                InterlockedAdd(second_pass_indirect_culled_boxes_parameters[0], 1, offset);
                            }
                        }
                    }
                }               
            }
        }
    }
};
