cbuffer ViewData : register(b1)
{
    float4x4 view_projection_matrix;
    float4x4 last_frame_view_projection_matrix;
    float4 camera_position;
    float4 time;
    float4 sun_direction;
    float4 frustum_planes[6];
    float4 frustum_points[8];
    float4 exposure;
    float4 bloom_01234;
    float4 bloom_5678;
};