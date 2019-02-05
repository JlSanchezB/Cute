//////////////////////////////////////////////////////////////////////////
// Cute engine - Render frame, a container with all the data needed from the game to render a scene
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_FRAME_H_
#define RENDER_FRAME_H_

#include <render/render_common.h>
#include <render/render_command_buffer.h>

namespace render
{
	//Minimal unit of render for cute
	//A render item represent a sorting key with a list of command for rendering
	//The meaning of priority is defined outside of rendering (a classic sample will be solid, lighting, alpha, ui, ...)
	struct Item
	{
		//Priority of the item (used for sorting them before rendering)
		uint32_t priority : 8;
		//Sort key inside the same priority
		uint32_t sort_key : 24;
		//Command buffer for rendering this item
		CommandBuffer::CommandOffset command_offset;
	};

	//Point of view, represent a list of render items
	//Each point of view has a priority and the render pass used for rendering it
	class PointOfView
	{
	public:
		void PushRenderItem(uint8_t priority, uint32_t sort_key, const CommandBuffer::CommandOffset& command_offset)
		{
			m_render_items.emplace_back(Item{ priority , sort_key, command_offset });
		}

		//Reset memory for next frame
		void Reset();

		PointOfView(PassName pass_name, uint16_t priority) : m_pass_name(pass_name), m_priority(priority), m_allocated(true)
		{
		}

	private:
		
		//Render pass that needs to be use for this point of view
		PassName m_pass_name;
		//Render priority
		uint16_t m_priority;
		//List of render items
		std::vector<Item> m_render_items;
		//Command buffer associated to this view
		CommandBuffer m_command_buffer;
		//Allocated
		bool m_allocated;

		friend class Frame;
	};

	//Render frame will keep memory between frames to avoid reallocations
	class Frame
	{
	public:

		//Reset the frame, it will not deallocate the memory, just clear the frame
		void Reset();

		//Alloc point of view
		PointOfView& AllocPointOfView(PassName pass_name, uint16_t priority);

	private:
		std::vector<PointOfView> m_point_of_views;
	};

	inline void PointOfView::Reset()
	{
		m_render_items.clear();
		m_command_buffer.Reset();
		m_allocated = false;
	}

	inline PointOfView& Frame::AllocPointOfView(PassName pass_name, uint16_t priority)
	{
		//Check it is already one that match from other frame
		for (auto& point_of_view : m_point_of_views)
		{
			if (!point_of_view.m_allocated && point_of_view.m_pass_name == pass_name && point_of_view.m_priority == priority)
			{
				point_of_view.m_allocated = true;
				return point_of_view;
			}
		}
		//Add to the vector
		m_point_of_views.emplace_back(pass_name, priority);
	}

	inline void Frame::Reset()
	{
		for (auto& point_of_view : m_point_of_views)
		{
			point_of_view.Reset();
		}
	}
}
#endif //RENDER_FRAME_H
