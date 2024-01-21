
CONTROL_VARIABLE(bool, BoxCulling, SkipSecondaryPassCulling, false)
CONTROL_VARIABLE(bool, BoxCulling, SecondaryPassCulling, true)
COUNTER(BoxCulling, SecondPassVisibleCubes)
COUNTER(BoxCulling, SecondPassTotalCubes)

cbuffer Root : register(b0)
{
    uint instance_lists_offset;
    uint indirect_box_buffer_count;
}

//Buffer with the offsets of all the culled boxes, the first element is the number of boxes in the second pass
StructuredBuffer<uint> second_pass_indirect_culled_boxes : register(t3);

//Indirect parameters for draw call
RWStructuredBuffer<uint> indirect_culled_boxes_parameters : register(u0);

//Buffer with the offsets of all the culled boxes
RWStructuredBuffer<uint> indirect_culled_boxes : register(u1);

#include "box_culling_common.hlsl"

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

    if (SkipSecondaryPassCulling)
    {
        return;
    }

    if (dispatch_thread_id.x < num_second_pass_boxes)
    {
        COUNTER_INC(SecondPassTotalCubes);
        //Read the box and do the culling with the HiZ

        uint indirect_box = second_pass_indirect_culled_boxes[dispatch_thread_id.x + 1];

        //Extract the instance list index, instance index and box index
        uint instance_list_index = (indirect_box >> 22) & 0x3FF;
        uint instance_index = (indirect_box >> 12) & 0x3FF;
        uint box_index = indirect_box & 0xFFF;

        //Read instance list offset and then the instance data offset
        uint instance_list_offset_byte = dynamic_gpu_memory.Load(instance_lists_offset + (1 + instance_list_index) * 4);
        uint instance_data_offset_byte = static_gpu_memory.Load(instance_list_offset_byte + (1 + instance_index) * 4);

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
       
        if (!hiz_is_visible(instance_rotate_quat, instance_translation, box_extent, box_translation) && SecondaryPassCulling)
        {
            //Occluded, nothing to do
            return;
        }
        else
        {
            //It is visible, added to the indirect box for the second pass render
            uint offset;
            InterlockedAdd(indirect_culled_boxes[0], 1, offset);

            if (offset < indirect_box_buffer_count)
            {
                indirect_culled_boxes[offset] = indirect_box;

                //Check if we need a new instance group
                if ((offset - 1) / 16 != (offset) / 16)
                {
                    uint offset;
                    InterlockedAdd(indirect_culled_boxes_parameters[1], 1, offset);
                }
            }

            COUNTER_INC(SecondPassVisibleCubes);
        }
    }
};
