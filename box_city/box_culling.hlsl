
uint instance_lists_offset : b0;
uint indirect_box_buffer_count : b1;

cbuffer ViewData : b2
{
    float4x4 view_projection_matrix;
    float4 time;
    float4 sun_direction;
};

ByteAddressBuffer static_gpu_memory: t0;
ByteAddressBuffer dynamic_gpu_memory: t1;

//Indirect parameters for draw call
RWStructuredBuffer<uint> indirect_culled_boxes_parameters : u2;

//Buffer with the offsets of all the culled boxes
RWStructuredBuffer<uint> indirect_culled_boxes : u3;


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
            uint instance_offset = static_gpu_memory.Load(instance_list_offset + 4 + instance_index * 4);

            //Read Box instance data
            float4 instance_data[4];

            instance_data[0] = asfloat(static_gpu_memory.Load4(instance_offset + 0));
            instance_data[1] = asfloat(static_gpu_memory.Load4(instance_offset + 16));
            instance_data[2] = asfloat(static_gpu_memory.Load4(instance_offset + 32));
            instance_data[3] = asfloat(static_gpu_memory.Load4(instance_offset + 48));

            float3x3 scale_rotate_matrix = float3x3(instance_data[0].xyz, instance_data[1].xyz, instance_data[2].xyz);
            float3x3 rotate_matrix = float3x3(normalize(instance_data[0].xyz), normalize(instance_data[1].xyz), normalize(instance_data[2].xyz));
            float3 translation = float3(instance_data[0].w, instance_data[1].w, instance_data[2].w);

            //Do culling

            //Add into the indirect buffer
            uint offset;
            InterlockedAdd(indirect_culled_boxes_parameters[1], 1, offset);
            if (offset < indirect_box_buffer_count)
            {
                indirect_culled_boxes[offset] = instance_offset;
            }
        }
    }
};
