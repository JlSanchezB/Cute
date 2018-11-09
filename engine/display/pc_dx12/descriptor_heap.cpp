#include "display_common.h"
#include "descriptor_heap.h"
#include <limits>

namespace display
{
	void DescriptorHeapPool::CreateHeap(Device * device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, size_t size)
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = static_cast<UINT>(size);
		rtvHeapDesc.Type = heap_type;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->m_native_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_descriptor_heap)));

		m_descriptor_size = device->m_native_device->GetDescriptorHandleIncrementSize(heap_type);
	}

	void DescriptorHeapPool::DestroyHeap()
	{
		SAFE_RELEASE(m_descriptor_heap);
	}

	void DescriptorHeapFreeList::CreateHeap(Device * device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, size_t size)
	{
		//Create heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = static_cast<UINT>(size);
		rtvHeapDesc.Type = heap_type;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->m_native_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_descriptor_heap)));

		m_descriptor_size = device->m_native_device->GetDescriptorHandleIncrementSize(heap_type);

		assert(size < std::numeric_limits<uint16_t>::max());

		//Create all descriptors are free
		Block all_free;
		all_free.index = 0;
		all_free.size = static_cast<uint16_t>(size);

	}
	void DescriptorHeapFreeList::DestroyHeap()
	{
		SAFE_RELEASE(m_descriptor_heap);
	}
	void DescriptorHeapFreeList::AllocDescriptors(Block & block, uint16_t num_descriptors)
	{
		//Find first free block in the list
		for (size_t i = 0; i < m_free_blocks_pool.size(); ++i)
		{
			auto& free_block = m_free_blocks_pool[i];
			if (free_block.size >= num_descriptors)
			{
				//Found it
				block.index = free_block.index;
				block.size = num_descriptors;

				//Left the rest as free
				if (free_block.size == num_descriptors)
				{
					//We use all the free block, swap with the back and remove
					free_block = m_free_blocks_pool.back();
					m_free_blocks_pool.pop_back();
				}
				else
				{
					//Keep the free part of the block as free
					free_block.index += num_descriptors;
					free_block.size -= num_descriptors;
				}
				return;
			}
		}
		//It doesn't fit in the list
		std::exception("No more free descriptors");
	}
	void DescriptorHeapFreeList::DeallocDescriptors(Block & block)
	{
		//Look inside the free blocks for blocks that connects with this block
		bool merge = false;
		for (size_t i = 0; i < m_free_blocks_pool.size(); ++i)
		{
			auto& free_block = m_free_blocks_pool[i];

			if ((free_block.index + free_block.size) == block.index)
			{
				//Free block and block are continuos, merge
				free_block.size += block.size;
				merge = true;
				break;
			}
			if ((block.index + block.size) == free_block.index)
			{
				//Block and free block are continuos, merge
				free_block.index = block.index;
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
		
		//Invalidate
		block.index = -1;
		block.size = -1;
	}
}