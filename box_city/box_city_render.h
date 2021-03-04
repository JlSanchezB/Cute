#ifndef BOX_CITY_RENDER_H
#define BOX_CITY_RENDER_H

#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include "resources.h"
#include <render_module/render_module_gpu_memory.h>

//Render pass definition for our custon box instance pass render
class DrawCityBoxItemsPass : public render::Pass
{
	uint8_t m_priority;
	inline static DisplayResource* m_display_resources;

	friend class BoxCityGame;
public:
	DECLARE_RENDER_CLASS("DrawCityBoxItems");

	void Load(render::LoadContext& load_context) override
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
	void Render(render::RenderContext& render_context) const override
	{
		//Collect all render items to render
		const render::PointOfView* point_of_view = render_context.GetPointOfView();
		if (point_of_view)
		{
			auto& sorted_render_items = point_of_view->GetSortedRenderItems();
			const size_t begin_render_item = sorted_render_items.m_priority_table[m_priority].first;
			const size_t end_render_item = sorted_render_items.m_priority_table[m_priority].second;

			if (begin_render_item < end_render_item)
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
				//Set the offset as a root constant
				context->SetConstants(display::Pipe::Graphics, 0, &offset_to_instance_offsets, 1);

				//Render
				display::WeakPipelineStateHandle box_pipeline_state = render::GetResource<render::GraphicsPipelineStateResource>(render_context.GetRenderSystem(), "BoxPipelineState"_sh32)->GetHandle();
				context->SetPipelineState(box_pipeline_state);

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
};

#endif //BOX_CITY_RENDER_H