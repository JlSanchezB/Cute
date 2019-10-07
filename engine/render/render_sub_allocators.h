//////////////////////////////////////////////////////////////////////////
// Cute engine - Helpers to handle sub allocations inside a render resource
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_SUB_ALLOCATORS_H_
#define RENDER_SUB_ALLOCATORS_H_

#include <core/ring_buffer.h>
#include <core/log.h>

namespace render
{
	//Ring buffer of resources sended to the GPU
	template<size_t RESOURCE_SIZE, size_t MAX_RESOURCES = 100>
	class SegmentAllocator
	{
	public:
		SegmentAllocator(size_t resource_size)
		{
		}
		size_t Alloc(size_t size, uint64_t allocation_frame_index, uint64_t freed_frame_index)
		{
			//Allocation is bigger than one resource segment
			assert(size < RESOURCE_SIZE);

			//Free all allocations until freed_frame_index
			while (!m_allocations.empty() && m_allocations.head().frame_index <= freed_frame_index)
			{
				m_allocations.pop();
			}

			
			//Check if has been an allocation for this frame before
			if (m_allocations.head().frame_index != allocation_frame_index)
			{
				auto& allocation = m_allocations.head();
				//There is space for it?
				if ((allocation.begin_allocation + allocation.size + size) > m_resource_size)
				{
					//Use the current allocation slot
					size_t ret_offset = allocation.size;
					allocation.size += size;

					//Update the next slot
					m_current_free = ret_offset + size;

					return m_allocations.GetBasePtr() + ret_offset;
				}
			}
			
			//Allocate a new frame allocation
			if (!m_allocations.full())
			{
				m_allocations.emplace(allocation_frame_index);

				auto& allocation = m_allocations.head();
				allocation.size = size;

				return m_allocations.GetBasePtr();
			}
			else
			{
				//GPU is really slow or too many information sent to the GPU
				//We can loop here until the GPU is free, but...
				core::LogError("SegmentAllocator ring buffer full, buffer size needs to be bigger");
				throw std::runtime_error("SegmentAllocator ring buffer full, buffer size needs to be bigger");
			}
		}
	private:
		struct FrameAllocation
		{
			//Frame that is going to be use
			uint64_t frame_index;
			//Current allocated size
			size_t size;

			FrameAllocation(uint64_t _frame_index) : frame_index(_frame_index)
			{
				size = 0;
			}
		};
		//List of allocations order by frame
		core::RingBuffer<FrameAllocation, MAX_RESOURCES> m_allocations;
	};
}
#endif //RENDER_SUB_ALLOCATORS_H_
