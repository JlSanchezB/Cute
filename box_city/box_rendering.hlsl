#include "constants.hlsl"

struct PSInput
{
    float4 view_position : SV_POSITION;
    nointerpolation float3 world_normal : TEXCOORD0;
    nointerpolation float4 colour : TEXCOORD1;
};
cbuffer Root : register(b0)
{
    uint instance_data_offset;
}

ByteAddressBuffer static_gpu_memory: register(t0);
ByteAddressBuffer dynamic_gpu_memory: register(t1);
StructuredBuffer<uint> indirect_box_buffer: register(t2);

float3 quat_multiplication(float4 quat, float3 vector)
{
    float3 qv = float3(quat.x, quat.y, quat.z);
    float s = -quat.w;
    float3 t = 2.0f * cross(qv, vector);
    return vector + (s * t) + cross(qv, t);
}

float3 quat_multiplication_transpose(float4 quat, float3 vector)
{
    float3 qv = float3(quat.x, quat.y, quat.z);
    float s = quat.w;
    float3 t = 2.0f * cross(qv, vector);
    return vector + (s * t) + cross(qv, t);
}

PSInput vs_box_main(uint multi_instance_id : SV_InstanceID, uint vertex_id : SV_VertexID)
{
    PSInput result;

    uint instance_id = multi_instance_id * 16 + vertex_id / 8;

    //Kill instances outside the number of instances using degenerated vertex
    if (instance_id >= (indirect_box_buffer[0] - 1))
    {
        result.view_position = float4(0.f, 0.f, 0.f, 1.f);
        result.world_normal = float3(0.f, 0.f, 0.f);
        result.colour = float4(0.f, 0.f, 0.f, 1.f);

        return result;
    }

    //The offset is in the indirect buffer
    uint indirect_box = indirect_box_buffer[instance_id + 1];

    //Extract the instance data
    uint instance_data_offset_byte = (indirect_box >> 8) * 16;

    //Read Box instance data
    float4 instance_data[3];

    instance_data[0] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 0));
    //instance_data[1] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 16));
    instance_data[2] = asfloat(static_gpu_memory.Load4(instance_data_offset_byte + 32));

    float4 instance_rotate_quat = instance_data[2];
    float3 instance_translation = float3(instance_data[0].x, instance_data[0].y, instance_data[0].z);

    //Then extract the box obb
    uint box_index = indirect_box & 0xFF;
    uint box_list_offset_byte = asuint(instance_data[0].w);
    box_list_offset_byte += 16; //Skip the box_list count
    box_list_offset_byte += box_index * 16 * 3; //Each box is a float4 * 3

    //Read Box data
    float4 box_data[3];
    box_data[0] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 0));
    box_data[1] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 16));
    box_data[2] = asfloat(static_gpu_memory.Load4(box_list_offset_byte + 32));

    float3 box_extent = float3(box_data[1].x, box_data[1].y, box_data[1].z);
    float3 box_translation = float3(box_data[0].x, box_data[0].y, box_data[0].z);
    float4 box_colour = float4(box_data[2].x, box_data[2].y, box_data[2].z, ((asuint(box_data[2].w) & 1) != 0) ? 1.f : 0.f); //Alpha 1.0 means emissive

    //Calculate the position from the camera
    //https://twitter.com/SebAaltonen/status/1315982782439591938
    
    //Camera vector local to the cube
    float3 camera_direction = camera_position.xyz - (quat_multiplication(instance_rotate_quat, box_translation) + instance_translation);
    float3 local_camera_direction = quat_multiplication_transpose(instance_rotate_quat, camera_direction);

    //Calculate position, 8 vertices per box
    uint3 box_xyz = uint3(vertex_id & 0x1, (vertex_id & 0x4) >> 2, (vertex_id & 0x2) >> 1);
    
    if (local_camera_direction.x > 0.f) box_xyz.x = 1 - box_xyz.x;
    if (local_camera_direction.y > 0.f) box_xyz.y = 1 - box_xyz.y;
    if (local_camera_direction.z > 0.f) box_xyz.z = 1 - box_xyz.z;

    float3 position = float3(box_xyz) * 2.f - 1.f;

    //Each position needs to be multiply by the local matrix
    float3 box_position = position * box_extent + box_translation;
    float3 world_position = quat_multiplication(instance_rotate_quat, box_position) + instance_translation;

    result.view_position = mul(view_projection_matrix, float4(world_position, 1.f));

    //vertex index to know the invoke vertex index
    //We know that all the face is going to use the normal of the invoke vertex and invoke vertex is not shared between the faces
    uint vertex_index = vertex_id % 8;
    if (vertex_index == 5) //Z
        result.world_normal = quat_multiplication(instance_rotate_quat, float3(0.f, 0.f, position.z));
    else if (vertex_index == 3) //Y
        result.world_normal = quat_multiplication(instance_rotate_quat, float3(0.f, position.y, 0.f));
    else if (vertex_index == 6) //X
        result.world_normal = quat_multiplication(instance_rotate_quat, float3(position.x, 0.f, 0.f));
    else
        result.world_normal = float3(0.f, 0.f, 0.f);

    result.colour = box_colour;

    return result;
}

float4 ps_box_main(PSInput input) : SV_TARGET
{
    if (input.colour.w == 1.f)
    {
        //Emissive
        return float4(input.colour.xyz, 1.f);
    }
    else
    {
        //Basic NdL
        float NL = (0.8f + 0.2f * saturate(dot(normalize(input.world_normal.xyz), sun_direction.xyz)));
        return float4(input.colour.xyz * NL, 1.f);
    }
}