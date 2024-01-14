struct ViewDataStruct
{
    float4x4 view_projection_matrix;
    float4x4 last_frame_view_projection_matrix;
    float4x4 view_projection_matrix_inv;
    float4 camera_position;
    float time;
    float3 time_gap;
    float4 sun_direction;
    float4 frustum_planes[6];
    float4 frustum_points[8];
    float exposure;
    float bloom_radius;
    float bloom_intensity;
    float gap;
    float fog_density;
    float3 fog_colour;
};