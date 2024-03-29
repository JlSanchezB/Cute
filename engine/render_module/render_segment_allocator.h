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
#include <core/profile.h>

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
			for (size_t i = init_allocated_segments; i > 0; --i)
			{
				m_free_allocations.push_back(i - 1);
			}

			OnResize(m_segment_count);
		}

		//Close frame cpu_frame_index
		//Free all frames before freed_frame_index, as GPU is done with the frame
		void Sync(uint64_t cpu_frame_index, uint64_t freed_frame_index);

		//Called when more memory is needed
		virtual void OnResize(size_t new_segment_count)
		{
		}

		//Allocs memory in the allocation for the frame index
		size_t Alloc(size_t size, uint64_t allocation_frame_index);

		//Allocs a full segment and keep it alive
		size_t AllocFullSegment(uint64_t allocation_frame_index);

		struct Stats
		{
			size_t m_memory_alive= 0; //Memory that is allocated and needed for all the frames in the GPU
		};

		//Collect stats
		void CollectStats(Stats& stats);

	private:

		//Allocs a full segment for this frame
		size_t AllocSegment(uint64_t allocation_frame_index);

		//An over approximation of max distance between CPU and GPU
		//That is from the GAME thread to the GPU
		static constexpr size_t kMaxFrames = 8;

		static constexpr size_t kInvalidSegment = static_cast<size_t>(-1);

		struct ActiveAllocation
		{
			size_t segment_index = kInvalidSegment;
			size_t current_size = 0;
		};

		struct Frame
		{
			//Frame index
			uint64_t frame_index = 0;

			//List of segments live in this frame
			std::vector<size_t> live_segments;

			//Current active allocations in this frame per thread
			job::ThreadData <ActiveAllocation> active_allocations;
		};

		//List of frames
		std::array<Frame, kMaxFrames> m_frames;

		//List of free allocation segment indexes
		std::vector<size_t> m_free_allocations;

		//Current segmented count used
		size_t m_segment_count;

		//Resource size
		size_t m_resource_size;

		//Segment size
		size_t m_segment_size;

		//Access mutex
		core::Mutex m_access_mutex;

		Frame& GetFrame(uint64_t frame_index)
		{
			//Use a ring buffer with the max distance frames between CPU and GPU
			auto& frame = m_frames[frame_index % kMaxFrames];

			if (frame.frame_index == 0)
			{
				//New frame, the current frame is not active, so all is OK
				frame.frame_index = frame_index;
			}
			else if (frame.frame_index != frame_index)
			{
				//the CPU and the GPU distance is higher than kMaxFrames
				core::LogError("Distance between CPU and GPU is over the max allocated, GPU blocked?");
				throw std::runtime_error("Distance between CPU and GPU is over the max allocated, GPU blocked?");
			}
			return frame;
		}
	};

	inline size_t SegmentAllocator::Alloc(size_t size, uint64_t allocation_frame_index)
	{
		PROFILE_SCOPE("Render", kRenderProfileColour, "SegmentAllocator::Alloc");
		assert(size > 0);
		assert(size <= m_segment_size);

		//Get frame
		auto& frame = GetFrame(allocation_frame_index);

		//Check if there is an allocation for this job thread and sufficient size
		auto& current_allocation = frame.active_allocations.Get();

		const bool first_allocation = current_allocation.segment_index == kInvalidSegment;
		const bool non_sufficient_memory = (current_allocation.segment_index != kInvalidSegment && ((current_allocation.current_size + size) >= m_segment_size));
		if (first_allocation || non_sufficient_memory)
		{
			//Check if there is an segment already active and send it to live allocations
			if (non_sufficient_memory)
			{
				core::MutexGuard guard(m_access_mutex);

				//We need to register the current allocation
				frame.live_segments.emplace_back(current_allocation.segment_index);
				current_allocation.segment_index = kInvalidSegment;
			}

			//Alloc a new segment
			{
				current_allocation.segment_index = AllocSegment(allocation_frame_index);
			}

			//A clean new allocation ready
			current_allocation.current_size = 0;
		}

		//Calculate the offset
		size_t allocation_offset = current_allocation.current_size + current_allocation.segment_index * m_segment_size;

		//Reserve
		current_allocation.current_size += size;

		//Return
		return allocation_offset;
	}

	inline size_t SegmentAllocator::AllocFullSegment(uint64_t allocation_frame_index)
	{
		size_t segment_index = AllocSegment(allocation_frame_index);

		GetFrame(allocation_frame_index).live_segments.emplace_back(segment_index);

		return segment_index;
	}

	inline size_t SegmentAllocator::AllocSegment(uint64_t allocation_frame_index)
	{
		core::MutexGuard guard(m_access_mutex);
		size_t segment_index;
		if (m_free_allocations.size() > 0)
		{
			segment_index = m_free_allocations.back();
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
			m_segment_count = std::min(old_count * 2, m_resource_size / m_segment_size);

			//Use the first new segment for the allocation
			segment_index = old_count;

			//Add the rest as free
			for (size_t i = m_segment_count - 1; i > old_count; --i)
			{
				m_free_allocations.push_back(m_segment_count - i + old_count);
			}

			OnResize(m_segment_count);
		}

		return segment_index;
	}

	inline void SegmentAllocator::Sync(uint64_t cpu_frame_index, uint64_t freed_frame_index)
	{
		core::MutexGuard guard(m_access_mutex);

		for (size_t i = 0; i < kMaxFrames; ++i)
		{
			auto& frame = m_frames[i];
			if (frame.frame_index > 0 && frame.frame_index <= freed_frame_index)
			{
				m_free_allocations.insert(m_free_allocations.end(), frame.live_segments.begin(), frame.live_segments.end());

				//Marked as completly free
				frame.frame_index = 0;
				frame.live_segments.clear();
			}
		}

		auto& clossing_frame = GetFrame(cpu_frame_index);
		//close all active allocations for cpu_frame_index
		clossing_frame.active_allocations.Visit([&](ActiveAllocation& allocation)
			{
				if (allocation.segment_index != kInvalidSegment)
				{
					//It was an allocation
					clossing_frame.live_segments.emplace_back(allocation.segment_index);

					//Allocation free
					allocation.current_size = 0;
					allocation.segment_index = kInvalidSegment;
				}
			});
	}

	inline void SegmentAllocator::CollectStats(Stats& stats)
	{
		stats = Stats();

		for (size_t i = 0; i < kMaxFrames; ++i)
		{
			auto& frame = m_frames[i];
			if (frame.frame_index > 0) //Check if the frame is active
			{
				stats.m_memory_alive += frame.live_segments.size() * m_segment_size;
				frame.active_allocations.Visit([&](ActiveAllocation& allocation)
					{
						stats.m_memory_alive += m_segment_size;
					});
			}
		}
	}
}
#endif //RENDER_SEGMENT_ALLOCATOR_H_
