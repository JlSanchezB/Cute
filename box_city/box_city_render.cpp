#include "box_city_render.h"
#include <display/display.h>

void CullCityBoxesPass::Render(render::RenderContext& render_context) const
{
	//Collect offsets from the point of view data
	const render::PointOfView* point_of_view = render_context.GetPointOfView();
	const BoxCityCustomPointOfViewData& box_city_custom_data = point_of_view->GetData<BoxCityCustomPointOfViewData>();
	
	if (box_city_custom_data.num_instance_lists == 0)
	{
		//Nothing to do
		return;
	}

	//Setup compute
	render_context.GetContext()->SetRootSignature(display::Pipe::Compute, m_display_resources->m_box_culling_root_signature);

	uint32_t constants[2] = { box_city_custom_data.instance_lists_offset, m_display_resources->kIndirectBoxBufferCount };

	render_context.GetContext()->SetConstants(display::Pipe::Compute, 0, constants, 2);
	render_context.GetContext()->SetDescriptorTable(display::Pipe::Compute, 1, m_display_resources->m_box_culling_description_table_handle);

	{
		//Clear
		render_context.GetContext()->SetPipelineState(m_display_resources->m_box_culling_clear_pipeline_state);

		display::ExecuteComputeDesc desc;
		desc.group_count_x = 1;
		desc.group_count_y = 1;
		desc.group_count_z = 1;
		render_context.GetContext()->ExecuteCompute(desc);

		std::vector<display::ResourceBarrier> resource_barriers;
		resource_barriers.emplace_back(m_display_resources->m_indirect_parameters_buffer);
		render_context.GetContext()->AddResourceBarriers(resource_barriers);
	}
	{
		//Culling
		render_context.GetContext()->SetPipelineState(m_display_resources->m_box_culling_pipeline_state);
		//Execute compute
		//First implementation, one group for each instance list
		display::ExecuteComputeDesc desc;
		desc.group_count_x = box_city_custom_data.num_instance_lists;
		desc.group_count_y = 1;
		desc.group_count_z = 1;
		render_context.GetContext()->ExecuteCompute(desc);
	}
}

void DrawCityBoxesPass::Render(render::RenderContext& render_context) const
{
	//The indirect box buffer and parameters buffers should have the correct information, we just need to call indirect drawcall
	auto context = render_context.GetContext();
	const render::PointOfView* point_of_view = render_context.GetPointOfView();
	const BoxCityCustomPointOfViewData& box_city_custom_data = point_of_view->GetData<BoxCityCustomPointOfViewData>();

	if (box_city_custom_data.num_instance_lists == 0)
	{
		//Nothing to do
		return;
	}

	context->SetRootSignature(display::Pipe::Graphics, m_display_resources->m_box_render_root_signature);
	//Set the offset as a root constant, zero means use the indirect path
	uint32_t offset_to_instance_offsets = 0;

	context->SetConstants(display::Pipe::Graphics, 0, &offset_to_instance_offsets, 1);
	context->SetDescriptorTable(display::Pipe::Graphics, 1, m_display_resources->m_box_render_description_table_handle);

	//Render
	context->SetPipelineState(m_display_resources->m_box_render_pipeline_state);

	display::WeakBufferHandle vertex_buffers[] = { m_display_resources->m_box_vertex_position_buffer, m_display_resources->m_box_vertex_normal_buffer };
	context->SetVertexBuffers(0, 1, &m_display_resources->m_box_vertex_position_buffer);
	context->SetVertexBuffers(1, 1, &m_display_resources->m_box_vertex_normal_buffer);
	context->SetIndexBuffer(m_display_resources->m_box_index_buffer);

	display::IndirectDrawIndexedInstancedDesc desc;
	desc.parameters_buffer = m_display_resources->m_indirect_parameters_buffer;
	context->IndirectDrawIndexedInstanced(desc);

}
