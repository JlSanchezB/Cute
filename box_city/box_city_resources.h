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
	glm::vec4 time;
	glm::vec4 sun_direction;
	glm::vec4 frustum_planes[6];
	glm::vec4 frustum_points[8];
};

struct BoxCityResources
{
	display::ConstantBufferHandle m_view_constant_buffer;
	display::DescriptorTableHandle m_box_render_description_table_handle;
	display::RootSignatureHandle m_box_render_root_signature;
	display::PipelineStateHandle m_box_render_pipeline_state;

	display::VertexBufferHandle m_box_vertex_position_buffer;
	display::VertexBufferHandle m_box_vertex_normal_buffer;
	display::IndexBufferHandle m_box_index_buffer;

	static const uint32_t kIndirectBoxBufferCount = 1024 * 1024;
	display::UnorderedAccessBufferHandle m_indirect_box_buffer;
	display::UnorderedAccessBufferHandle m_indirect_parameters_buffer;
	display::DescriptorTableHandle m_box_culling_description_table_handle;
	display::RootSignatureHandle m_box_culling_root_signature;
	display::PipelineStateHandle m_box_culling_pipeline_state;
	display::PipelineStateHandle m_box_culling_clear_pipeline_state;

	void Load(display::Device* device, render::System* render_system);
	void Unload(display::Device* device);
};

#endif //BOX_CITY_RESOURCES
