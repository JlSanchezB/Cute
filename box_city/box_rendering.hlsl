struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : TEXCOORD0;
    float4 colour : TEXCOORD1;
};
cbuffer Root : register(b0)
{
    uint instance_data_offset;
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
StructuredBuffer<uint> indirect_box_buffer: register(t2);

PSInput vs_box_main(float3 position : POSITION, float3 normal : NORMAL, uint instance_id : SV_InstanceID)
{
    PSInput result;

    //The offset is in the indirect buffer
    uint indirect_box = indirect_box_buffer[instance_id];

    //Extract the instance data
    uint instance_data_offset_byte = (indirect_box >> 8) * 16;

    //Read Box instance data
    float4 instance_data[4];

    instance_data[0] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 0));
    instance_data[1] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 16));
    instance_data[2] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 32));
    instance_data[3] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 48));
    
    float3x3 instance_rotate_matrix = float3x3(instance_data[1].xyz, instance_data[2].xyz, instance_data[3].xyz);
    float3 instance_translation = float3(instance_data[0].x, instance_data[0].y, instance_data[0].z);

    //Then extract the box obb
    uint box_index = indirect_box & 0xFF;
    uint box_list_offset_byte = asuint(instance_data[0].w);
    box_list_offset_byte += 16; //Skip the box_list count
    box_list_offset_byte += box_index * 16 * 5; //Each box is a float4 * 5

    //Read Box data
    float4 box_data[5];
    box_data[0] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 0));
    box_data[1] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 16));
    box_data[2] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 32));
    box_data[3] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 48));
    box_data[4] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 64));

    float3x3 box_rotate_matrix = float3x3(box_data[1].xyz, box_data[2].xyz, box_data[3].xyz);
    float3 box_extent = float3(box_data[1].w, box_data[2].w, box_data[3].w);
    float3 box_translation = float3(box_data[0].x, box_data[0].y, box_data[0].z);
    float4 box_colour = float4(box_data[4].x, box_data[4].y, box_data[4].z, 1.f);

    //Each position needs to be multiply by the local matrix
    float3 box_position = mul(box_rotate_matrix, position * box_extent) + box_translation;
    float3 world_position = mul(instance_rotate_matrix, box_position) + instance_translation;

    result.position = mul(view_projection_matrix, float4(world_position, 1.f));

    float3 box_normal = mul(box_rotate_matrix, normal);
    result.normal = mul(instance_rotate_matrix, box_normal);
    result.colour = box_colour;

    return result;
}

float4 ps_box_main(PSInput input) : SV_TARGET
{
    return float4(input.colour.xyz, 1.f) * (0.3f + 0.7f * saturate(dot(normalize(input.normal), sun_direction.xyz)));
}