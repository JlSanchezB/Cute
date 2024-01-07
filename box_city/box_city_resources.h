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
	glm::vec4 camera_position;
	glm::vec4 time;
	glm::vec4 sun_direction;
	glm::vec4 frustum_planes[6];
	glm::vec4 frustum_points[8];
	glm::vec4 exposure;
	glm::vec4 bloom_0123;
	glm::vec4 bloom_4567;
};

struct BoxCityResources
{
	display::BufferHandle m_view_constant_buffer;
	display::DescriptorTableHandle m_box_render_description_table_handle;
	display::RootSignatureHandle m_box_render_root_signature;
	display::PipelineStateHandle m_box_render_pipeline_state;

	display::BufferHandle m_box_index_buffer;

	static const uint32_t kIndirectBoxBufferCount = 5 * 1024 * 1024;
	static const uint32_t kSecondPassIndirectBoxBufferCount = 5 * 1024 * 1024;
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
