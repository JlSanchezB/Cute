//////////////////////////////////////////////////////////////////////////
// Cute engine - FreeListAllocator, allocates memory inside a buffer in the GPU
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_FREELIST_ALLOCATOR_H_
#define RENDER_FREELIST_ALLOCATOR_H_

#include <core/ring_buffer.h>
#include <core/log.h>
#include <job/job_helper.h>
#include <core/sync.h>
#include <vector>
#include <utility>

#define RENDER_FREELIST_VALIDATE
namespace render
{
	struct AllocateListAllocation
	{
		size_t offset;
		size_t size;
	};

	using AllocHandle = core::Handle<AllocateListAllocation, uint32_t>;
	using WeakAllocHandle = core::WeakHandle<AllocateListAllocation, uint32_t>;

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
			m_handle_pool.Init(1000000, 100);

			//Add free block
			m_first_free_block = 0;
			m_free_block_pool.emplace_back(0, m_resource_size);
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
		void Dealloc(AllocHandle& handle, uint64_t last_used_frame_index);

		//Access to the handle data
		AllocateListAllocation& Get(const WeakAllocHandle& handle)
		{
			return m_handle_pool[handle];
		}

		//Access to the handle data
		const AllocateListAllocation& Get(const WeakAllocHandle& handle) const
		{
			return m_handle_pool[handle];
		}

	private:

		//An over approximation of max distance between CPU and GPU
		//That is from the GAME thread to the GPU
		static constexpr size_t kMaxFrames = 8;

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

		//Handle pool of allocated blocks
		core::HandlePool<AllocHandle> m_handle_pool;

		static constexpr uint32_t kInvalidFreeBlock = static_cast<uint32_t>(-1);
		struct FreeListFreeAllocation
		{
			size_t offset;
			size_t size;

			uint32_t prev = kInvalidFreeBlock;
			uint32_t next = kInvalidFreeBlock;

			FreeListFreeAllocation(size_t _offset, size_t _size) : offset(_offset), size(_size)
			{
			}
		};

		//Vector with all the free blocks
		std::vector<FreeListFreeAllocation> m_free_block_pool;

		//First free block
		uint32_t m_first_free_block;

		//Access mutex
		core::Mutex m_access_mutex;

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
				core::LogError("Distance between CPU and GPU is over the max allocated, GPU blocked?");
				throw std::runtime_error("Distance between CPU and GPU is over the max allocated, GPU blocked?");
			}
			return frame;
		}
		void RemoveFreeBlock(uint32_t index_to_remove)
		{
			auto& block_to_free = m_free_block_pool[index_to_remove];
			//Fix linked list
			if (block_to_free.prev != kInvalidFreeBlock)
			{
				m_free_block_pool[block_to_free.prev].next = block_to_free.next;
			}
			else
			{
				//Ok, this block was the first free, so now it is the next
				m_first_free_block = block_to_free.next;
				assert(m_first_free_block != kInvalidFreeBlock);
			}

			if (block_to_free.next != kInvalidFreeBlock)
			{
				m_free_block_pool[block_to_free.next].prev = block_to_free.prev;
			}

			//Deallocate, swap and pop

			//We need to copy the last to the free slot and fix the linked list
			if (index_to_remove != m_free_block_pool.size() - 1)
			{
				if (m_first_free_block == m_free_block_pool.size() - 1)
				{
					m_first_free_block = index_to_remove;
				}

				//Copy the last block over the index to remove
				m_free_block_pool[index_to_remove] = m_free_block_pool.back();
				auto& swap_block = m_free_block_pool[index_to_remove];

				//Fix linked list
				if (swap_block.prev != kInvalidFreeBlock)
				{
					m_free_block_pool[swap_block.prev].next = index_to_remove;
				}

				if (swap_block.next != kInvalidFreeBlock)
				{
					m_free_block_pool[swap_block.next].prev = index_to_remove;
				}
			}
			//And pop the last
			m_free_block_pool.pop_back();
		}
		uint32_t InsertPrev(uint32_t index_to_insert, size_t offset, size_t size)
		{
			m_free_block_pool.emplace_back(offset, size);
			uint32_t new_free_index = static_cast<uint32_t>(m_free_block_pool.size() - 1);
			FreeListFreeAllocation& new_free_block = m_free_block_pool.back();

			FreeListFreeAllocation& reference_free_block = m_free_block_pool[index_to_insert];

			assert(new_free_block.offset < reference_free_block.offset);

			//Fix the linked list
			if (reference_free_block.prev != kInvalidFreeBlock)
			{
				m_free_block_pool[reference_free_block.prev].next = new_free_index;
			}
			else
			{
				assert(m_first_free_block == index_to_insert);
				//You are the new first
				m_first_free_block = new_free_index;
			}
			new_free_block.next = index_to_insert;
			new_free_block.prev = reference_free_block.prev;
			reference_free_block.prev = new_free_index;

			assert(new_free_block.next != new_free_block.prev);

			return new_free_index;
		}

		void CheckBlockForMerge(uint32_t new_free_index)
		{
			//Check if it can be merged with the previous or the next, or the two

			bool prev_merge = false;
			bool next_merge = false;

			FreeListFreeAllocation& new_free_allocation = m_free_block_pool[new_free_index];

			if (new_free_allocation.prev != kInvalidFreeBlock)
			{
				FreeListFreeAllocation& prev_free_allocation = m_free_block_pool[new_free_allocation.prev];

				if (prev_free_allocation.offset + prev_free_allocation.size == new_free_allocation.offset)
				{
					//That can be merged
					prev_merge = true;
				}
			}

			if (new_free_allocation.next != kInvalidFreeBlock)
			{
				FreeListFreeAllocation& next_free_allocation = m_free_block_pool[new_free_allocation.next];

				if (new_free_allocation.offset + new_free_allocation.size == next_free_allocation.offset)
				{
					//That can be merged
					next_merge = true;
				}
			}

			//We the previous, but merge all of them
			if (prev_merge && next_merge)
			{
				//We can delete new and the next
				uint32_t prev_index = new_free_allocation.prev;
				uint32_t next_index = new_free_allocation.next;
				FreeListFreeAllocation& prev_free_allocation = m_free_block_pool[prev_index];
				FreeListFreeAllocation& next_free_allocation = m_free_block_pool[next_index];
				
				//Add the sizes to the previous
				prev_free_allocation.size += new_free_allocation.size + next_free_allocation.size;

				//Deallocate current and next
				RemoveFreeBlock(new_free_index);
				RemoveFreeBlock(next_index);
			}
			else if (prev_merge)
			{
				//We delete new and merge it to the prev
				uint32_t prev_index = new_free_allocation.prev;
				FreeListFreeAllocation& prev_free_allocation = m_free_block_pool[prev_index];

				//Add the sizes
				prev_free_allocation.size += new_free_allocation.size;

				//Deallocate current
				RemoveFreeBlock(new_free_index);
			}
			else if (next_merge)
			{
				//We delete the next one and keep the new
				uint32_t next_index = new_free_allocation.next;
				FreeListFreeAllocation& next_free_allocation = m_free_block_pool[next_index];

				//Add the sizes
				new_free_allocation.size += next_free_allocation.size;

				//Deallocate next
				RemoveFreeBlock(next_index);
			}
			//Nothing to merge
		}
#ifdef RENDER_FREELIST_VALIDATE
		//Check for issues in the allocator
		void Validate()
		{	
			//Test that all inside the free list is in order and is merged
			uint32_t current_index = m_first_free_block;
			while (current_index != kInvalidFreeBlock)
			{
				assert(current_index < m_free_block_pool.size());
				auto& current_block = m_free_block_pool[current_index];
				assert(current_block.next != current_block.prev || current_block.next == kInvalidFreeBlock);
				assert(current_block.size > 0);
				assert(m_first_free_block != current_index || current_block.prev == kInvalidFreeBlock);
				if (current_block.prev != kInvalidFreeBlock)
				{
					//Is not merged and in order
					auto& prev_block = m_free_block_pool[current_block.prev];

					assert(prev_block.offset < current_block.offset); //Order
					assert(prev_block.offset + prev_block.size < current_block.offset); //Not merged
				}
				if (current_block.next != kInvalidFreeBlock)
				{
					//Is not merged and in order
					auto& next_block = m_free_block_pool[current_block.next];

					assert(next_block.offset > current_block.offset); //Order
					assert(current_block.offset + current_block.size < next_block.offset); //Not merged
				}

				current_index = current_block.next;
			}
		}
	};
#endif
	inline AllocHandle FreeListAllocator::Alloc(size_t size)
	{
		assert(size > 0);
		assert(size < m_resource_size);

		//Always align the size to 16
		size = (((size - 1) >> 4) + 1) << 4;

		core::MutexGuard guard(m_access_mutex);

		uint32_t free_index = m_first_free_block;

		while (free_index != kInvalidFreeBlock)
		{
			auto& free_block = m_free_block_pool[free_index];
			if (free_block.size >= size)
			{
				//Check if it is the same size or not
				if (free_block.size == size)
				{
					//Create the allocation
					AllocateListAllocation allocation_block{ free_block.offset, free_block.size };

					//Remove handle from free blocks
					RemoveFreeBlock(free_index);
					
					//Create alloc handle
					return m_handle_pool.Alloc(allocation_block);
				}
				else
				{
					AllocateListAllocation new_allocation;
					new_allocation.offset = free_block.offset;
					new_allocation.size = size;

					//Split the free block, the back the free
					free_block.offset += size;
					free_block.size -= size;

					//We don't need to create or delete new block, as the blocks only handle the free handles
					return m_handle_pool.Alloc(new_allocation);
				}
			}
			free_index = free_block.next;
		}

		//It doesn't fit in the list
		core::LogError("No more free allocations in the free list render allocator");
		throw std::runtime_error("No more free allocations in the free list render allocator");
	}

	inline void FreeListAllocator::Dealloc(AllocHandle& handle, uint64_t last_used_frame_index)
	{
		assert(handle.IsValid());

		core::MutexGuard guard(m_access_mutex);

		auto& frame = GetLiveDeallocationsFrame(last_used_frame_index);

		//Add deallocation into our deallocation ring buffer
		frame.handles.emplace_back(std::move(handle));
	}

	inline void FreeListAllocator::Sync(uint64_t freed_frame_index)
	{
		core::MutexGuard guard(m_access_mutex);

		//Free all allocations until freed_frame_index
		for (size_t i = 0; i < kMaxFrames; ++i)
		{
			auto& frame = m_live_deallocations[i];
			if (frame.frame_index > 0 && frame.frame_index <= freed_frame_index)
			{
				//Add to the free list
				for (auto&& handle : frame.handles)
				{
					AllocateListAllocation& deallocated_block = m_handle_pool[handle];

					//All the free blocks are linked list in order in the memory, we need to keep that order
					uint32_t reference_free_index = m_first_free_block;
					while (reference_free_index != kInvalidFreeBlock)
					{
						assert(m_free_block_pool[reference_free_index].offset != deallocated_block.offset);
						if (m_free_block_pool[reference_free_index].offset > deallocated_block.offset)
						{
							break;
						}
						reference_free_index = m_free_block_pool[reference_free_index].next;
					}
					//Always found one, as the last one is the rest of the buffer, we can not go right to it
					assert(reference_free_index != kInvalidFreeBlock);

					//We need to add it just before the free_block
					uint32_t new_free_index = InsertPrev(reference_free_index, deallocated_block.offset, deallocated_block.size);

					//Deallocate the handle
					m_handle_pool.Free(handle);

					CheckBlockForMerge(new_free_index);
				}

				//Mark this frame as completly free
				frame.frame_index = 0;
				frame.handles.clear();
			}
		}

#ifdef RENDER_FREELIST_VALIDATE
		Validate();
#endif
	}
}
#endif //RENDER_FREELIST_ALLOCATOR_H_
