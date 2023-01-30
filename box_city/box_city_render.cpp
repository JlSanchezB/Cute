#include "box_city_render.h"
#include <display/display.h>

void DrawCityBoxesPass::Load(render::LoadContext& load_context)
{
}

void DrawCityBoxesPass::Destroy(display::Device* device)
{
}

void DrawCityBoxesPass::Render(render::RenderContext& render_context) const
{
	//Collect offsets from the point of view data
	const render::PointOfView* point_of_view = render_context.GetPointOfView();
	const BoxCityCustomPointOfViewData& box_city_custom_data = point_of_view->GetData<BoxCityCustomPointOfViewData>();
	
	
	//Clear the parameters buffer

	//Execute GPU compute

	//Add transition

	//Render
}

void DrawCityBoxItemsPass::Load(render::LoadContext& load_context)
{
	const char* value;
	if (load_context.current_xml_element->QueryStringAttribute("priority", &value) == tinyxml2::XML_SUCCESS)
	{
		m_priority = GetRenderItemPriority(load_context.render_system, render::PriorityName(value));
	}
	else
	{
		AddError(load_context, "Attribute priority expected inside DrawCityBoxItems pass");
	}
}
void DrawCityBoxItemsPass::Destroy(display::Device* device)
{
}
void DrawCityBoxItemsPass::Render(render::RenderContext& render_context) const
{
	//Collect all render items to render
	const render::PointOfView* point_of_view = render_context.GetPointOfView();
	if (point_of_view)
	{
		auto& sorted_render_items = point_of_view->GetSortedRenderItems();
		const size_t begin_render_item = sorted_render_items.m_priority_table[m_priority].first;
		const size_t end_render_item = sorted_render_items.m_priority_table[m_priority].second;

		if (begin_render_item != -1)
		{
			//Allocate a buffer that for each box has an offset to the box data
			auto* gpu_memory = render::GetModule<render::GPUMemoryRenderModule>(render_context.GetRenderSystem(), "GPUMemory"_sh32);

			uint32_t* instances_ptrs = reinterpret_cast<uint32_t*>(gpu_memory->AllocDynamicGPUMemory(render_context.GetDevice(), (end_render_item - begin_render_item + 1) * sizeof(int32_t), render::GetRenderFrameIndex(render_context.GetRenderSystem())));

			//Upload all the instances offsets
			for (size_t render_item_index = begin_render_item; render_item_index <= end_render_item; ++render_item_index)
			{
				//Just copy the offset
				instances_ptrs[render_item_index - begin_render_item] = sorted_render_items.m_sorted_render_items[render_item_index].data;
			}

			uint32_t offset_to_instance_offsets = static_cast<uint32_t>(gpu_memory->GetDynamicGPUMemoryOffset(render_context.GetDevice(), instances_ptrs));

			auto context = render_context.GetContext();

			context->SetRootSignature(display::Pipe::Graphics, m_display_resources->m_box_render_root_signature);
			//Set the offset as a root constant
			context->SetConstants(display::Pipe::Graphics, 0, &offset_to_instance_offsets, 1);
			context->SetDescriptorTable(display::Pipe::Graphics, 1, m_display_resources->m_box_render_description_table_handle);

			//Render
			context->SetPipelineState(m_display_resources->m_box_render_pipeline_state);

			display::WeakVertexBufferHandle vertex_buffers[] = { m_display_resources->m_box_vertex_position_buffer, m_display_resources->m_box_vertex_normal_buffer };
			context->SetVertexBuffers(0, 1, &m_display_resources->m_box_vertex_position_buffer);
			context->SetVertexBuffers(1, 1, &m_display_resources->m_box_vertex_normal_buffer);
			context->SetIndexBuffer(m_display_resources->m_box_index_buffer);

			display::DrawIndexedInstancedDesc desc;
			desc.instance_count = static_cast<uint32_t>((end_render_item - begin_render_item + 1));
			desc.index_count = 36;
			context->DrawIndexedInstanced(desc);
		}
	}
}
