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
	using PointOfViewName = StringHash32<"PointOfViewName"_namespace>;

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
		
		Item()
		{
		}

		explicit Item(Priority _priority, SortKey _sort_key, const CommandBuffer::CommandOffset& _command_offset) :
			priority(_priority), sort_key(_sort_key), command_offset(_command_offset), command_worker(static_cast<uint32_t>(job::GetWorkerIndex()))
		{
		}

		explicit Item(Priority _priority, SortKey _sort_key, const uint32_t& _data) :
			priority(_priority), sort_key(_sort_key), data(_data)
		{
		}
	};

	//Sorted render items
	struct SortedRenderItems
	{
		//Sorted render items list
		std::vector<Item> m_sorted_render_items;
		//Index access to the sorted render items by priority (begin item and end item)
		std::vector<std::pair<size_t, size_t>> m_priority_table;
	};

	//Point of view, represent a list of render items
	//Each point of view has a priority and the render pass used for rendering it
	class PointOfView
	{
	public:
		PointOfView(PointOfViewName point_of_view_name, uint16_t id, const void* data, size_t data_size) :
			m_name(point_of_view_name), m_id(id), m_allocated(true)
		{
			//Capture data
			if (data != nullptr && data_size > 0)
			{
				m_data = std::make_unique<std::byte[]>(data_size);
				memcpy(m_data.get(), data, data_size);
			}
		}

		void PushRenderItem(Priority priority, SortKey sort_key, const CommandBuffer::CommandOffset& command_offset)
		{
			assert(sort_key < (1 << 24));
			m_render_items.Get().emplace_back(priority , sort_key, command_offset);
		}

		void PushRenderItem(Priority priority, SortKey sort_key, const uint32_t& data)
		{
			assert(sort_key < (1 << 24));
			m_render_items.Get().emplace_back(priority, sort_key, data);
		}

		CommandBuffer& GetCommandBuffer()
		{
			return m_command_buffer.Get();
		}

		//Reset memory for next frame
		void Reset();

		//Get Sorted Render Items, only accessible from a pass
		const SortedRenderItems& GetSortedRenderItems() const
		{
			return m_sorted_render_items;
		}

		//Get data associated to this point of view
		template<class DATA>
		const DATA& GetData() const
		{
			return *reinterpret_cast<DATA*>(m_data.get());
		};

	private:
		
		//Point of view name, used for identification
		PointOfViewName m_name;
		//ID, used for identification
		uint16_t m_id;

		//List of render items
		job::ThreadData<std::vector<Item>> m_render_items;
		//Command buffer associated to this view
		job::ThreadData<CommandBuffer> m_command_buffer;
		//Allocated
		bool m_allocated;

		//Sorted render items associated (updated by the render system)
		SortedRenderItems m_sorted_render_items;

		//Custom data associated to this point of view
		std::unique_ptr<std::byte[]> m_data;

		friend class Frame;
		friend struct System;
		friend class DrawRenderItemsPass;
	};

	//Render pass that the render needs to execute
	struct RenderPass
	{
		PassName pass_name;
		uint16_t id; //Used for split screens or shadows
		PassInfo pass_info;
		PointOfViewName associated_point_of_view_name;
		uint16_t associated_point_of_view_id;
	};

	struct EmptyData
	{
	};

	//Render frame will keep memory between frames to avoid reallocations
	class Frame
	{
	public:

		//Reset the frame, it will not deallocate the memory, just clear the frame
		void Reset();

		//Alloc point of view with custom with name and id to identify it
		template<class DATA>
		PointOfView& AllocPointOfView(PointOfViewName point_of_view_name, uint16_t id, const DATA& data);

		//Alloc point of view with name and id to identify it
		PointOfView& AllocPointOfView(PointOfViewName point_of_view_name, uint16_t id)
		{
			return AllocPointOfView<EmptyData>(point_of_view_name, id, EmptyData{});
		}

		//Add render pass to execute
		void AddRenderPass(PassName pass_name, uint16_t id, const PassInfo& pass_info, PointOfViewName associated_point_of_view_name = PointOfViewName("None"), uint16_t associated_point_of_view_id = 0)
		{
			m_render_passes.push_back({ pass_name, id, pass_info, associated_point_of_view_name, associated_point_of_view_id });
		}

		//Get begin frame command buffer
		CommandBuffer& GetBeginFrameCommandBuffer()
		{
			return m_begin_frame_command_buffer.Get();
		}
	private:
		//List of all point of views in the frame
		std::list<PointOfView> m_point_of_views;
		//List of all the render passes that the renderer needs to execute
		std::vector<RenderPass> m_render_passes;
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
		
		m_allocated = false;
	}

	template<class DATA>
	inline PointOfView& Frame::AllocPointOfView(PointOfViewName point_of_view_name, uint16_t id, const DATA& data)
	{
		//Check it is already one that match from other frame
		for (auto& point_of_view : m_point_of_views)
		{
			if (!point_of_view.m_allocated &&
				point_of_view.m_name == point_of_view_name
				&& point_of_view.m_id == id)
			{
				point_of_view.m_allocated = true;
				return point_of_view;
			}
		}
		//Add to the vector
		m_point_of_views.emplace_back(point_of_view_name, id, &data, sizeof(DATA));

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
		m_render_passes.clear();
	}
}
#endif //RENDER_FRAME_H
