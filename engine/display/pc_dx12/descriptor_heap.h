//////////////////////////////////////////////////////////////////////////
// Cute engine - Descriptor Heap
//////////////////////////////////////////////////////////////////////////
#ifndef DESCRIPTOR_HEAP_H_
#define DESCRIPTOR_HEAP_H_

namespace display
{
	struct Device;

	//A heap pool helper, one index, one descriptor
	class DescriptorHeapPool
	{
	public:
		inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(size_t index) const;
		inline CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(size_t index) const;
	protected:
		void CreateHeap(Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, size_t size);
		void DestroyHeap();
	private:
		ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
		UINT m_descriptor_size;
	};

	//A heap free list helper, one index, several descriptors
	class DescriptorHeapFreeList
	{
	public:

	protected:
		void CreateHeap(Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, size_t size);
		void DestroyHeap();

		struct Block
		{
			uint16_t index;
			uint16_t size;
		};

		void AllocDescriptors(Block& block, uint16_t num_descriptors);
		void DeallocDescriptors(Block& block);

		inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(const Block& item) const;
		inline CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(const Block& item) const;
	private:

		ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
		UINT m_descriptor_size;

		//Pool of free blocks
		std::vector<Block> m_free_blocks_pool;
	};

	template<typename BASE>
	struct DescriptorHeapFreeListItem : public BASE, public DescriptorHeapFreeList::Block
	{
	};

	inline CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeapPool::GetDescriptor(size_t index) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptor_size);
	}
	inline CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeapPool::GetGPUDescriptor(size_t index) const
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptor_size);
	}

	inline CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeapFreeList::GetDescriptor(const Block& item) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(item.index), m_descriptor_size);
	}
	inline CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeapFreeList::GetGPUDescriptor(const Block& item) const
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(item.index), m_descriptor_size);
	}
}
#endif