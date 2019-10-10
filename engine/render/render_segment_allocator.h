//////////////////////////////////////////////////////////////////////////
// Cute engine - Segmented allocator, used for allocating memory in segments for the GPU
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_SEGMENT_ALLOCATOR_H_
#define RENDER_SEGMENT_ALLOCATOR_H_

#include <core/ring_buffer.h>
#include <core/log.h>
#include <job/job_helper.h>
#include <core/sync.h>
#include <vector>

namespace render
{
	//Allocates memory for each job thread in segments
	class SegmentAllocator
	{
	public:

		virtual ~SegmentAllocator()
		{
		}

		void Init(size_t resource_size, size_t segment_size, size_t init_allocated_segments = 16)
		{
			assert(resource_size % segment_size == 0);
			assert(init_allocated_segments > 0);

			m_resource_size = resource_size;
			m_segment_size = segment_size;
			m_segment_count = init_allocated_segments;

			//Init the free list
			for (size_t i = 0; i < init_allocated_segments; ++i)
			{
				m_free_allocations.push_back(i);
			}

			OnResize(m_segment_count);

			//Reset the allocations
			m_active_allocations.Reset();
		}

		//Freed live allocations will avaliable
		//Close open allocations
		void Sync(uint64_t freed_frame_index)
		{
			core::SpinLockMutexGuard guard(m_access_mutex);

			//Free all allocations until freed_frame_index
			while (!m_live_allocations.empty() && m_live_allocations.head().frame_index <= freed_frame_index)
			{
				//Add to the free list
				m_free_allocations.push_back(m_live_allocations.head().segment_index);
				m_live_allocations.pop();
			}

			//Add all current allocations
			m_active_allocations.Visit([&](CurrentAllocation& allocation)
				{
					if (allocation.frame_index != 0)
					{
						//It was an allocation
						m_live_allocations.emplace(allocation.frame_index, allocation.segment_index);

						//Allocation free
						allocation.frame_index = 0;
						allocation.current_size = 0;
					}
				});
		}

		//Called when more memory is needed
		virtual void OnResize(size_t new_segment_count)
		{
		}

		//Allocs memory in the allocation for the frame index
		size_t SegmentAllocator::Alloc(size_t size, uint64_t allocation_frame_index);

	private:
		struct FrameAllocation
		{
			//Frame that is going to be use
			uint64_t frame_index;
			//Allocation segment index
			size_t segment_index;

			FrameAllocation()
			{
			};
			FrameAllocation(uint64_t _frame_index, size_t _segment_index) : frame_index(_frame_index), segment_index(_segment_index)
			{
			}
		};

		//List of live allocations, head old ones and tail new ones
		core::GrowableRingBuffer<FrameAllocation> m_live_allocations;

		struct CurrentAllocation
		{
			uint64_t frame_index = 0;
			size_t segment_index;
			size_t current_size;
		};

		//Current allocations by job thread
		job::ThreadData<CurrentAllocation> m_active_allocations;

		//List of free allocation segment indexes
		std::vector<size_t> m_free_allocations;

		//Current segmented count used
		size_t m_segment_count;

		//Resource size
		size_t m_resource_size;

		//Segment size
		size_t m_segment_size;

		//Access mutex
		core::SpinLockMutex m_access_mutex;
	};

	inline size_t SegmentAllocator::Alloc(size_t size, uint64_t allocation_frame_index)
	{
		assert(size == 0);
		assert(size < m_segment_size);

		//Check if there is an allocation for this job thread and sufficient size
		auto& current_allocation = m_active_allocations.Get();

		const bool old_frame_index_allocation = current_allocation.frame_index != allocation_frame_index;
		const bool non_sufficient_memory = (current_allocation.frame_index == allocation_frame_index && (current_allocation.current_size + size >= m_segment_size));
		if (old_frame_index_allocation || non_sufficient_memory)
		{
			//Check if there is an segment already active
			if (old_frame_index_allocation)
			{
				if (current_allocation.frame_index != 0)
				{
					core::SpinLockMutexGuard guard(m_access_mutex);

					//We need to register the current allocation
					m_live_allocations.emplace(current_allocation.frame_index, current_allocation.segment_index);
				}
			}

			//Alloc a new segment
			{
				core::SpinLockMutexGuard guard(m_access_mutex);
				if (m_free_allocations.size() > 0)
				{
					current_allocation.segment_index = m_free_allocations.back();
					m_free_allocations.pop_back();
				}
				else
				{
					//We need to reserve more segments
					if (m_segment_count == m_resource_size / m_segment_size)
					{
						//Out of memory, there is no memory for allocating more inside the resource
						core::LogError("Segment allocation out of memory");
						throw std::runtime_error("Segment allocation out of memory");
					}

					size_t old_count = m_segment_count;
					m_segment_count = std::max(old_count * 2, m_resource_size / m_segment_size);

					//Use the first new segment for the allocation
					current_allocation.segment_index = old_count;

					//Add the rest as free
					for (size_t i = old_count + 1; i < m_segment_count; ++i)
					{
						m_free_allocations.push_back(i);
					}

					OnResize(m_segment_count);
				}
			}

			//A clean new allocation ready
			current_allocation.current_size = 0;
			current_allocation.frame_index = allocation_frame_index;
		}

		//Calculate the offset
		size_t allocation_offset = current_allocation.current_size + current_allocation.segment_index * m_segment_size;

		//Reserve
		current_allocation.current_size += size;

		//Return
		return allocation_offset;
	}
}
#endif //RENDER_SEGMENT_ALLOCATOR_H_
