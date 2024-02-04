//////////////////////////////////////////////////////////////////////////
// Cute engine - resources for the ECS test
//////////////////////////////////////////////////////////////////////////
#ifndef BOX_CITY_RESOURCES_h
#define BOX_CITY_RESOURCES_h

#include <display/display.h>
#include <ext/glm/glm.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>

namespace render
{
	struct System;
}

struct ViewConstantBuffer
{
	glm::mat4x4 projection_view_matrix;
	glm::mat4x4 last_frame_view_projection_matrix;
	glm::mat4x4 projection_view_matrix_inv;
	glm::vec4 camera_position;
	float time;
	float elapse_time;
	float resolution_x;
	float resolution_y;
	glm::vec4 sun_direction;
	glm::vec4 frustum_planes[6];
	glm::vec4 frustum_points[8];
	float exposure;
	float bloom_radius;
	float bloom_intensity;
	float gap_1;
	float fog_density;
	glm::vec3 fog_colour;
	float fog_top_height;
	float fog_bottom_height;
	glm::vec2 gap_2;
};

struct BoxCityResources
{
	display::BufferHandle m_view_constant_buffer;
	display::DescriptorTableHandle m_box_render_description_table_handle;
	display::RootSignatureHandle m_box_render_root_signature;
	display::PipelineStateHandle m_box_render_pipeline_state;

	display::BufferHandle m_box_index_buffer;

	static const uint32_t kIndirectBoxBufferCount = 10 * 1024 * 1024;
	static const uint32_t kSecondPassIndirectBoxBufferCount = 10 * 1024 * 1024;
	display::BufferHandle m_indirect_box_buffer;
	display::BufferHandle m_second_pass_indirect_box_buffer;
	display::BufferHandle m_indirect_parameters_buffer;
	display::BufferHandle m_second_pass_indirect_parameters_buffer;
	display::DescriptorTableHandle m_box_culling_description_table_handle;
	display::RootSignatureHandle m_box_culling_root_signature;
	display::PipelineStateHandle m_box_culling_pipeline_state;
	display::RootSignatureHandle m_second_pass_box_culling_root_signature;
	display::PipelineStateHandle m_second_pass_box_culling_pipeline_state;
	display::DescriptorTableHandle m_second_pass_box_culling_description_table_handle;
	display::PipelineStateHandle m_box_culling_clear_pipeline_state;
	display::PipelineStateHandle m_second_pass_box_culling_clear_pipeline_state;

	void Load(display::Device* device, render::System* render_system);
	void Unload(display::Device* device);
};

#endif //BOX_CITY_RESOURCES
