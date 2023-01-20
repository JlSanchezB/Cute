struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : TEXCOORD0;
    float4 colour : TEXCOORD1;
};

cbuffer ViewData : b0
{
    float4x4 view_projection_matrix;
    float4 time;
    float4 sun_direction;
};

cbuffer Root : b1
{
    uint instance_data_offset;
}

StructuredBuffer<float4> static_gpu_memory: t0;
StructuredBuffer<float4> dynamic_gpu_memory: t1;

PSInput vs_box_main(float3 position : POSITION, float3 normal : NORMAL, uint instance_id : SV_InstanceID)
{
    PSInput result;

    //Get box instance data using primitiveId and the instance_data_offset
    uint instance_offset_floats = instance_data_offset / 4 + instance_id;
    uint instance_data_offset_byte = asuint(dynamic_gpu_memory[instance_offset_floats / 4][instance_offset_floats % 4]);

    //Read Box instance data
    float4 instance_data[4];

    instance_data[0] = static_gpu_memory[(instance_data_offset_byte / 16) + 0];
    instance_data[1] = static_gpu_memory[(instance_data_offset_byte / 16) + 1];
    instance_data[2] = static_gpu_memory[(instance_data_offset_byte / 16) + 2];
    instance_data[3] = static_gpu_memory[(instance_data_offset_byte / 16) + 3];

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