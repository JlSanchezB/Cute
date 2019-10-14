//////////////////////////////////////////////////////////////////////////
// Cute engine - FreeListAllocator, allocates memory inside a buffer in the GPU
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_FREELIST_ALLOCATOR_H_
#define RENDER_FREELIST_ALLOCATOR_H_

#include <core/handle_pool.h>
#include <core/ring_buffer.h>
#include <core/log.h>
#include <job/job_helper.h>
#include <core/sync.h>
#include <vector>
#include <utility>

namespace render
{
	class FreeListAllocation
	{
		size_t offset;
		size_t size;

		friend class FreeListAllocator;
	};

	using AllocHandle = core::Handle<FreeListAllocation, uint16_t>;
	using WeakAllocHandle = core::WeakHandle<FreeListAllocation, uint16_t>;

	//Allocates memory inside a GPU resource and returns a handle
	//Deferres deallocations when it is dellocated
	class FreeListAllocator
	{
	public:

		virtual ~FreeListAllocator()
		{
		}

		void Init(size_t resource_size)
		{
			m_resource_size = resource_size;

			//Init the handle pool
			m_handle_pool.Init(10000, 100);
		}

		//Freed live allocations will avaliable
		//Close open allocations
		void Sync(uint64_t freed_frame_index);

		//Called when more memory is needed
		virtual void OnResize(size_t new_segment_count)
		{
		}

		//Allocs memory in the allocation for the frame index
		AllocHandle Alloc(size_t size);

		//Deallocate memory with last frame used
		void Dealloc(AllocHandle&& handle, uint64_t last_used_frame_index);

	private:

		//An over approximation of max distance between CPU and GPU
		//That is from the GAME thread to the GPU
		static constexpr size_t kMaxFrames = 8;

		//List of free blocks
		std::vector<FreeListAllocation> m_free_blocks_pool;

		struct LiveDeallocation
		{
			size_t frame_index = 0;

			//List all deallocations that can happen when that frame is done
			std::vector<AllocHandle> handles;
		};

		//List of live dellocations per frame
		std::array<LiveDeallocation, kMaxFrames> m_live_deallocations;

		//Resource size
		size_t m_resource_size;

		//Handle pool
		core::HandlePool<AllocHandle> m_handle_pool;

		//Access mutex
		core::SpinLockMutex m_access_mutex;

		LiveDeallocation& GetLiveDeallocationsFrame(uint64_t frame_index)
		{
			//Use a ring buffer with the max distance frames between CPU and GPU
			auto& frame = m_live_deallocations[frame_index % kMaxFrames];

			if (frame.frame_index == 0)
			{
				//New frame, the current frame is not active, so all is OK
				frame.frame_index = frame_index;
			}
			else if (frame.frame_index != frame_index)
			{
				//the CPU and the GPU distance is higher than kMaxFrames
				core::LogError("Distance between CPU and GPU is higher that max, GPU blocked?");
				throw std::runtime_error("Distance between CPU and GPU is higher that max, GPU blocked?");
			}
			return frame;
		}
	};

	inline AllocHandle FreeListAllocator::Alloc(size_t size)
	{
		assert(size == 0);
		assert(size < m_resource_size);

		//Always align the size to 16
		size = ((size << 4) + 1) >> 4;

		core::SpinLockMutexGuard guard(m_access_mutex);

		//Get a allocation for our free list
		//Find first free block in the list
		FreeListAllocation allocation;

		for (size_t i = 0; i < m_free_blocks_pool.size(); ++i)
		{
			auto& free_block = m_free_blocks_pool[i];
			if (free_block.size >= size)
			{
				//Found it
				allocation.offset = free_block.offset;
				allocation.size = size;

				//Left the rest as free
				if (free_block.size == size)
				{
					//We use all the free block, swap with the back and remove
					free_block = m_free_blocks_pool.back();
					m_free_blocks_pool.pop_back();
				}
				else
				{
					//Keep the free part of the block as free
					free_block.offset += size;
					free_block.size -= size;
				}

				//Alloc handle in pool 
				return m_handle_pool.Alloc(allocation);
			}
		}

		//It doesn't fit in the list
		core::LogError("No more free allocations in the free list render allocator");
		throw std::runtime_error("No more free allocations in the free list render allocator");
	}

	inline void FreeListAllocator::Dealloc(AllocHandle&& handle, uint64_t last_used_frame_index)
	{
		assert(handle.IsValid());

		core::SpinLockMutexGuard guard(m_access_mutex);

		auto& frame = GetLiveDeallocationsFrame(last_used_frame_index);

		//Add deallocation into our deallocation ring buffer
		frame.handles.emplace_back(std::move(handle));
	}

	inline void FreeListAllocator::Sync(uint64_t freed_frame_index)
	{
		core::SpinLockMutexGuard guard(m_access_mutex);

		//Check if the deallocations are not used in the gpu

		//Free all allocations until freed_frame_index
		for (size_t i = 0; i < kMaxFrames; ++i)
		{
			auto& frame = m_live_deallocations[i];
			if (frame.frame_index > 0 && frame.frame_index <= freed_frame_index)
			{
				//Add to the free list
				for (auto&& handle : frame.handles)
				{
					FreeListAllocation& block = m_handle_pool[handle];

					//Look inside the free blocks for blocks that connects with this block
					bool merge = false;
					for (size_t i = 0; i < m_free_blocks_pool.size(); ++i)
					{
						auto& free_block = m_free_blocks_pool[i];

						if ((free_block.offset + free_block.size) == block.offset)
						{
							//Free block and block are continuos, merge
							free_block.size += block.size;
							merge = true;
							break;
						}
						if ((block.offset + block.size) == block.offset)
						{
							//Block and free block are continuos, merge
							free_block.offset = block.offset;
							free_block.size += block.size;
							merge = true;
							break;
						}
					}

					if (!merge)
					{
						//add it as a free block
						m_free_blocks_pool.push_back(block);
					}
				}

				//Mark this frame as completly free
				frame.frame_index = 0;
			}
		}
	}
}
#endif //RENDER_FREELIST_ALLOCATOR_H_
