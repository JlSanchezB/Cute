#include "display_common.h"
#include "descriptor_heap.h"

namespace display
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeapPool::GetDescriptor(size_t index) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptor_size);
	}

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
}