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

ByteAddressBuffer static_gpu_memory: t0;
ByteAddressBuffer dynamic_gpu_memory: t1;

