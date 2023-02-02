struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : TEXCOORD0;
    float4 colour : TEXCOORD1;
};
cbuffer Root : b0
{
    uint instance_data_offset;
}
cbuffer ViewData : b1
{
    float4x4 view_projection_matrix;
    float4 time;
    float4 sun_direction;
};


ByteAddressBuffer static_gpu_memory: t0;
ByteAddressBuffer dynamic_gpu_memory: t1;
StructuredBuffer<uint> indirect_box_buffer: t2;

PSInput vs_box_main(float3 position : POSITION, float3 normal : NORMAL, uint instance_id : SV_InstanceID)
{
    PSInput result;

    uint instance_data_offset_byte = 0;
    if (instance_data_offset == 0)
    {
        //The offset is in the indirect buffer
        instance_data_offset_byte = indirect_box_buffer[instance_id];
    }
    else
    {
        //Get box instance data using primitiveId and the instance_data_offset
        uint instance_offset_byte = instance_data_offset + instance_id * 4;
        instance_data_offset_byte = dynamic_gpu_memory.Load(instance_offset_byte);
    }

    //Read Box instance data
    float4 instance_data[4];

    instance_data[0] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 0));
    instance_data[1] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 16));
    instance_data[2] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 32));
    instance_data[3] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 48));

    float3x3 scale_rotate_matrix = float3x3(instance_data[0].xyz, instance_data[1].xyz, instance_data[2].xyz);
    float3x3 rotate_matrix = float3x3(normalize(instance_data[0].xyz), normalize(instance_data[1].xyz), normalize(instance_data[2].xyz));
    float3 translation = float3(instance_data[0].w, instance_data[1].w, instance_data[2].w);

    //Each position needs to be multiply by the local matrix
    float3 world_position = mul(scale_rotate_matrix, position) + translation;
    result.position = mul(view_projection_matrix, float4(world_position, 1.f));

    result.normal = mul(rotate_matrix, normal);
    result.colour = instance_data[3];

    return result;
}

float4 ps_box_main(PSInput input) : SV_TARGET
{
    return float4(input.colour.xyz, 1.f) * (0.2f + 0.8f * saturate(dot(normalize(input.normal), sun_direction.xyz)));
}