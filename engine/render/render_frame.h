//////////////////////////////////////////////////////////////////////////
// Cute engine - Render frame, a container with all the data needed from the game to render a scene
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_FRAME_H_
#define RENDER_FRAME_H_

#include <render/render_common.h>
#include <render/render_command_buffer.h>
#include <list>
#include <job/job_helper.h>

namespace render
{
	//Minimal unit of render for cute
	//A render item represent a sorting key with a list of command for rendering
	//The meaning of priority is defined outside of rendering (a classic sample will be solid, lighting, alpha, ui, ...)
	struct Item
	{
		union
		{
			struct
			{
				//Priority of the item (used for sorting them before rendering)
				uint32_t priority : 8;
				//Sort key inside the same priority
				uint32_t sort_key : 24;
			};
			
			//Fast access to the 32bits sort key used for sorting items
			uint32_t full_32bit_sort_key;
		};
		union
		{
			//Command buffer access
			struct
			{
				//Command buffer for rendering this item
				uint32_t command_offset : 24;
				//Worker that built the commands
				uint32_t command_worker : 8;
			};

			//Generic 32 bits data associated to the render item
			uint32_t data;
		};
		

		Item(Priority _priority, SortKey _sort_key, const CommandBuffer::CommandOffset& _command_offset) :
			priority(_priority), sort_key(_sort_key), command_offset(_command_offset), command_worker(static_cast<uint32_t>(job::GetWorkerIndex()))
		{
		}
	};

	//Point of view, represent a list of render items
	//Each point of view has a priority and the render pass used for rendering it
	class PointOfView
	{
	public:
		PointOfView(PassName pass_name, uint16_t id, uint16_t priority, const PassInfo& pass_info) :
			m_pass_name(pass_name), m_id(id), m_priority(priority), m_allocated(true), m_pass_info(pass_info)
		{
		}

		void PushRenderItem(Priority priority, SortKey sort_key, const CommandBuffer::CommandOffset& command_offset)
		{
			m_render_items.Get().emplace_back(priority , sort_key, command_offset);
		}

		CommandBuffer& GetCommandBuffer()
		{
			return m_command_buffer.Get();
		}

		CommandBuffer& GetBeginRenderCommandBuffer()
		{
			return m_begin_render_command_buffer.Get();
		}

		//Reset memory for next frame
		void Reset();

	private:
		
		//Render pass that needs to be use for this point of view
		PassName m_pass_name;
		//ID, use for handling between frames data of the point of view
		uint16_t m_id;
		//Priority, used to sort the render between different point of views
		uint16_t m_priority;
		//Pass info
		PassInfo m_pass_info;

		//Command buffer with commands that will run during the start of the point of view
		job::ThreadData <CommandBuffer> m_begin_render_command_buffer;

		//List of render items
		job::ThreadData<std::vector<Item>> m_render_items;
		//Command buffer associated to this view
		job::ThreadData<CommandBuffer> m_command_buffer;
		//Allocated
		bool m_allocated;

		friend class Frame;
		friend struct System;
		friend class DrawRenderItemsPass;
	};

	//Render frame will keep memory between frames to avoid reallocations
	class Frame
	{
	public:

		//Reset the frame, it will not deallocate the memory, just clear the frame
		void Reset();

		//Alloc point of view
		PointOfView& AllocPointOfView(PassName pass_name, uint16_t id, uint16_t priority, const PassInfo& pass_info);

		//Get begin frame command buffer
		CommandBuffer& GetBeginFrameComamndbuffer()
		{
			return m_begin_frame_command_buffer.Get();
		}
	private:
		//List of all point of views in the frame
		std::list<PointOfView> m_point_of_views;
		//Command buffer with commands that will run during the start of the frame
		job::ThreadData<CommandBuffer> m_begin_frame_command_buffer;

		friend struct System;
	};

	inline void PointOfView::Reset()
	{
		//Reset all the data for all the workers
		m_render_items.Visit([](auto& data)
		{
			data.clear();
		});
		m_command_buffer.Visit([](auto& data)
		{
			data.Reset();
		});
		m_begin_render_command_buffer.Visit([](auto& data)
		{
			data.Reset();
		});
		m_allocated = false;
	}

	inline PointOfView& Frame::AllocPointOfView(PassName pass_name, uint16_t id, uint16_t priority, const PassInfo& pass_info)
	{
		//Check it is already one that match from other frame
		for (auto& point_of_view : m_point_of_views)
		{
			if (!point_of_view.m_allocated &&
				point_of_view.m_pass_name == pass_name
				&& point_of_view.m_id == id)
			{
				point_of_view.m_priority = priority;
				point_of_view.m_pass_info = pass_info;
				point_of_view.m_allocated = true;
				return point_of_view;
			}
		}
		//Add to the vector
		m_point_of_views.emplace_back(pass_name, id, priority, pass_info);

		return m_point_of_views.back();
	}

	inline void Frame::Reset()
	{
		for (auto& point_of_view : m_point_of_views)
		{
			point_of_view.Reset();
		}
		m_begin_frame_command_buffer.Visit([](auto& data)
		{
			data.Reset();
		});
	}
}
#endif //RENDER_FRAME_H
