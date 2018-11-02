//////////////////////////////////////////////////////////////////////////
// Cute engine - Descriptor Heap
//////////////////////////////////////////////////////////////////////////
#ifndef DESCRIPTOR_HEAP_H_
#define DESCRIPTOR_HEAP_H_

namespace display
{
	struct Device;

	class DescriptorHeapPool
	{
	public:
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(size_t index) const;
	protected:
		void CreateHeap(Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, size_t size);
		void DestroyHeap();
	private:
		ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
		UINT m_descriptor_size;
	};
}
#endif