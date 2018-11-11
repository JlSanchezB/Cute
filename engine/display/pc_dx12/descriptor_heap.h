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
		inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(size_t index, size_t heap = 0) const;
		inline CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(size_t index, size_t heap = 0) const;
	protected:
		void AddHeap(Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, size_t size);
		void DestroyHeaps();
	private:
		struct DescriptorHeap
		{
			ComPtr<ID3D12DescriptorHeap> heap;
			UINT descriptor_size;
		};
		std::vector<DescriptorHeap> m_descriptor_heap;
	};

	//A heap free list helper, one index, several descriptors
	class DescriptorHeapFreeList
	{
	public:
		struct Block
		{
			uint16_t index;
			uint16_t size;
		};

		ID3D12DescriptorHeap* GetHeap() const
		{
			return m_descriptor_heap.Get();
		}

	protected:
		void CreateHeap(Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, size_t size);
		void DestroyHeap();

		void AllocDescriptors(Block& block, uint16_t num_descriptors);
		void DeallocDescriptors(Block& block);

		inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(const Block& item, size_t offset = 0) const;
		inline CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(const Block& item, size_t offset = 0) const;

	private:

		ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
		UINT m_descriptor_size;

		//Pool of free blocks
		std::vector<Block> m_free_blocks_pool;
	};

	inline CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeapPool::GetDescriptor(size_t index, size_t heap) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap[heap].heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptor_heap[heap].descriptor_size);
	}
	inline CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeapPool::GetGPUDescriptor(size_t index, size_t heap) const
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptor_heap[heap].heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptor_heap[heap].descriptor_size);
	}

	inline CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeapFreeList::GetDescriptor(const Block& item, size_t offset) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(item.index + offset), m_descriptor_size);
	}
	inline CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeapFreeList::GetGPUDescriptor(const Block& item, size_t offset) const
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(item.index + offset), m_descriptor_size);
	}
}
#endif