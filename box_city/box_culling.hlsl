cbuffer ViewData : b0
{
    float4x4 view_projection_matrix;
    float4 time;
    float4 sun_direction;
};

uint instance_lists_offset : b1;
uint indirect_box_buffer_count : b2;

ByteAddressBuffer static_gpu_memory: t0;
ByteAddressBuffer dynamic_gpu_memory: t1;

//Buffer with the offsets of all the culled boxes
RWStructuredBuffer<uint> indirect_culled_boxes : u2;
//Indirect parameters for draw call
RWStructuredBuffer<uint> indirect_culled_boxes_parameters : u3;


//Naive implementation for culling
//Each group will get one instance list
[numthreads(32, 1, 1)]
void box_culling(uint3 thread : SV_DispatchThreadID)
{
    if (thread.x == 0)
    {
        indirect_culled_boxes_parameters[0] = 0;
    }
};
