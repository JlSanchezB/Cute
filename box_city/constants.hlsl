struct ViewDataStruct
{
    float4x4 view_projection_matrix;
    float4x4 last_frame_view_projection_matrix;
    float4x4 view_projection_matrix_inv;
    float4 camera_position;
    float time;
    float elapse_time;
    float resolution_x;
    float resolution_y;
    float4 sun_direction;
    float4 frustum_planes[6];
    float4 frustum_points[8];
    float exposure;
    float bloom_radius;
    float bloom_intensity;
    float gap_1;
    float fog_density;
    float3 fog_colour;
    float fog_top_height;
    float fog_bottom_height;
    float2 gap_2;
};