#include "box_city_render.h"
#include <display/display.h>

void CullCityBoxesPass::Render(render::RenderContext& render_context) const
{
	//Collect offsets from the point of view data
	const render::PointOfView* point_of_view = render_context.GetPointOfView();
	const BoxCityCustomPointOfViewData& box_city_custom_data = point_of_view->GetData<BoxCityCustomPointOfViewData>();
	auto* gpu_memory = render::GetModule<render::GPUMemoryRenderModule>(render_context.GetRenderSystem());

	if (box_city_custom_data.num_instance_lists == 0)
	{
		//Nothing to do
		return;
	}

	//Setup compute
	render_context.GetContext()->SetRootSignature(display::Pipe::Compute, m_display_resources->m_box_culling_root_signature);

	uint32_t constants[3] = { box_city_custom_data.instance_lists_offset, m_display_resources->kIndirectBoxBufferCount, m_display_resources->kSecondPassIndirectBoxBufferCount };

	render_context.GetContext()->SetConstants(display::Pipe::Compute, 0, constants, 3);

	//Update the descriptor table
	display::DescriptorTableDesc::Descritor descriptors[8];
	descriptors[0] = m_display_resources->m_view_constant_buffer;
	descriptors[1] = gpu_memory->GetStaticGPUMemoryResource();
	descriptors[2] = gpu_memory->GetDynamicGPUMemoryResource();
	descriptors[3] = std::get<display::WeakTexture2DHandle>(render::GetResource(render_context.GetRenderSystem(), "HiZ"_sh32)->GetDisplayHandle());
	descriptors[4] = display::AsUAVBuffer(m_display_resources->m_indirect_parameters_buffer);
	descriptors[5] = display::AsUAVBuffer(m_display_resources->m_indirect_box_buffer);
	descriptors[6] = display::AsUAVBuffer(m_display_resources->m_second_pass_indirect_parameters_buffer);
	descriptors[7] = display::AsUAVBuffer(m_display_resources->m_second_pass_indirect_box_buffer);

	display::UpdateDescriptorTable(render_context.GetDevice(), m_display_resources->m_box_culling_description_table_handle, descriptors, 8);

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
		resource_barriers.emplace_back(m_display_resources->m_indirect_box_buffer);
		resource_barriers.emplace_back(m_display_resources->m_second_pass_indirect_parameters_buffer);
		resource_barriers.emplace_back(m_display_resources->m_second_pass_indirect_box_buffer);
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

void CullSecondPassCityBoxesPass::Render(render::RenderContext& render_context) const
{
	//Collect offsets from the point of view data
	const render::PointOfView* point_of_view = render_context.GetPointOfView();
	const BoxCityCustomPointOfViewData& box_city_custom_data = point_of_view->GetData<BoxCityCustomPointOfViewData>();
	auto* gpu_memory = render::GetModule<render::GPUMemoryRenderModule>(render_context.GetRenderSystem());

	//Setup compute
	render_context.GetContext()->SetRootSignature(display::Pipe::Compute, m_display_resources->m_second_pass_box_culling_root_signature);

	uint32_t constants[2] = { box_city_custom_data.instance_lists_offset, m_display_resources->kIndirectBoxBufferCount };

	render_context.GetContext()->SetConstants(display::Pipe::Compute, 0, constants, 2);

	//Update the descriptor table
	display::DescriptorTableDesc::Descritor descriptors[7];
	descriptors[0] = m_display_resources->m_view_constant_buffer;
	descriptors[1] = gpu_memory->GetStaticGPUMemoryResource();
	descriptors[2] = gpu_memory->GetDynamicGPUMemoryResource();
	descriptors[3] = std::get<display::WeakTexture2DHandle>(render::GetResource(render_context.GetRenderSystem(), "HiZ"_sh32)->GetDisplayHandle());
	descriptors[4] = m_display_resources->m_second_pass_indirect_box_buffer;
	descriptors[5] = display::AsUAVBuffer(m_display_resources->m_indirect_parameters_buffer);
	descriptors[6] = display::AsUAVBuffer(m_display_resources->m_indirect_box_buffer);

	display::UpdateDescriptorTable(render_context.GetDevice(), m_display_resources->m_second_pass_box_culling_description_table_handle, descriptors, 7);

	render_context.GetContext()->SetDescriptorTable(display::Pipe::Compute, 1, m_display_resources->m_second_pass_box_culling_description_table_handle);

	{
		//Clear
		render_context.GetContext()->SetPipelineState(m_display_resources->m_second_pass_box_culling_clear_pipeline_state);

		display::ExecuteComputeDesc desc;
		desc.group_count_x = 1;
		desc.group_count_y = 1;
		desc.group_count_z = 1;
		render_context.GetContext()->ExecuteCompute(desc);

		std::vector<display::ResourceBarrier> resource_barriers;
		resource_barriers.emplace_back(m_display_resources->m_indirect_parameters_buffer);
		resource_barriers.emplace_back(m_display_resources->m_indirect_box_buffer);
		render_context.GetContext()->AddResourceBarriers(resource_barriers);
	}
	{
		//Culling
		render_context.GetContext()->SetPipelineState(m_display_resources->m_second_pass_box_culling_pipeline_state);
		//Execute indirect using the parameters buffer generated from the first pass
		display::IndirectExecuteComputeDesc desc;
		desc.parameters_buffer = m_display_resources->m_second_pass_indirect_parameters_buffer;
		render_context.GetContext()->IndirectExecuteCompute(desc);
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

	uint32_t constants[1] = { box_city_custom_data.instance_lists_offset};
	render_context.GetContext()->SetConstants(display::Pipe::Graphics, 0, constants, 1);
	context->SetDescriptorTable(display::Pipe::Graphics, 1, m_display_resources->m_box_render_description_table_handle);

	//Render
	context->SetPipelineState(m_display_resources->m_box_render_pipeline_state);

	context->SetIndexBuffer(m_display_resources->m_box_index_buffer);

	display::IndirectDrawIndexedInstancedDesc desc;
	desc.parameters_buffer = m_display_resources->m_indirect_parameters_buffer;
	context->IndirectDrawIndexedInstanced(desc);

}
