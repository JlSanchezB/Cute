#include "display_common.h"

namespace
{
	//Create a commited resource
	void CreateResource(display::Device* device, void* data, size_t size, bool static_buffer, ComPtr<ID3D12Resource>& resource)
	{
		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;

		if (static_buffer)
		{
			ThrowIfFailed(device->m_native_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(size),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&resource)));
		}
		else
		{
			//The resource will be the upload_resource
			upload_resource = resource;
		}
		
		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload_resource)));

		if (static_buffer)
		{
			// Copy data to the intermediate upload heap and then schedule a copy 
			// from the upload heap to the vertex buffer.
			D3D12_SUBRESOURCE_DATA copy_data = {};
			copy_data.pData = data;
			copy_data.RowPitch = size;
			copy_data.SlicePitch = size;

			//Command list
			auto& command_list = device->Get(device->m_resource_command_list).resource;

			//Open command list
			OpenCommandList(device, device->m_resource_command_list);

			UpdateSubresources<1>(command_list.Get(), resource.Get(), upload_resource.Get(), 0, 0, 1, &copy_data);
			command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

			//Close command list
			CloseCommandList(device, device->m_resource_command_list);

			//Execute the command list
			ExecuteCommandList(device, device->m_resource_command_list);

			//The upload resource is not needed, add to the deferred resource buffer
			AddDeferredDeleteResource(device, upload_resource);
		}
		else
		{
			//Copy to the upload heap
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
			ThrowIfFailed(resource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, data, size);
			resource->Unmap(0, nullptr);
		}
	}
}

namespace display
{
	//Delete resources that are not needed by the GPU
	size_t DeletePendingResources(display::Device* device)
	{
		if (device->m_resource_deferred_delete_ring_buffer.empty())
		{
			return 0;
		}

		size_t count = 0;
		for (; !device->m_resource_deferred_delete_ring_buffer.empty();)
		{
			//Get the head of the ring buffer
			auto& head = device->m_resource_deferred_delete_ring_buffer.head();

			//Current GPU fence value
			UINT64 gpu_fence_value = device->m_resource_deferred_delete_fence->GetCompletedValue();

			//Check if the GPU got the fence
			if (head.fence_value <= gpu_fence_value)
			{
				//This resource is out of scope, the GPU doens't need it anymore, delete it
				device->m_resource_deferred_delete_ring_buffer.pop();
				count++;
			}
			else
			{
				//We don't have anything more to delete, the GPU still needs the resource
				break;
			}
		}
		return count;
	}

	//Add a resource to be deleted, only to be called if you are sure that you don't need the resource
	void AddDeferredDeleteResource(display::Device* device, ComPtr<ID3D12Object> resource)
	{
		//Look if there is space
		if (device->m_resource_deferred_delete_ring_buffer.full())
		{
			//BAD, the ring buffer is full, we need to make space deleted resources that the GPU doesn't need
			size_t count = DeletePendingResources(device);

			if (count == 0)
			{
				//really BAD, the GPU still needs all the resources
				//Sync to a event and wait
				UINT64 fence_value_to_wait = device->m_resource_deferred_delete_ring_buffer.head().fence_value;

				ThrowIfFailed(device->m_resource_deferred_delete_fence->SetEventOnCompletion(fence_value_to_wait, device->m_resource_deferred_delete_event));
				WaitForSingleObjectEx(device->m_resource_deferred_delete_event, INFINITE, FALSE);

				//Delete the resource
				DeletePendingResources(device);
			}
		}

		//At this moment we have space in the ring

		//Add the resource to the ring buffer
		device->m_resource_deferred_delete_ring_buffer.emplace(resource, device->m_resource_deferred_delete_index);
		//Signal the GPU, so the GPU will update the fence when it reach here
		device->m_command_queue->Signal(device->m_resource_deferred_delete_fence.Get(), device->m_resource_deferred_delete_index);
		//Increase the fence value
		device->m_resource_deferred_delete_index++;
	}

	VertexBufferHandle CreateVertexBuffer(Device * device, void* data, size_t stride, size_t size)
	{
		VertexBufferHandle handle = device->m_vertex_buffer_pool.Alloc();

		auto& vertex_buffer = device->Get(handle);
		
		CreateResource(device, data, size, true, vertex_buffer.resource);

		// Initialize the vertex buffer view.
		vertex_buffer.view.BufferLocation = vertex_buffer.resource->GetGPUVirtualAddress();
		vertex_buffer.view.StrideInBytes = static_cast<UINT>(stride);
		vertex_buffer.view.SizeInBytes = static_cast<UINT>(size);

		return handle;
	}

	void DestroyVertexBuffer(Device * device, VertexBufferHandle & handle)
	{
		//Delete handle
		device->m_vertex_buffer_pool.Free(handle);
	}

	IndexBufferHandle CreateIndexBuffer(Device * device, void* data, size_t size, Format format)
	{
		IndexBufferHandle handle = device->m_index_buffer_pool.Alloc();

		auto& index_buffer = device->Get(handle);

		CreateResource(device, data, size, true, index_buffer.resource);

		// Initialize the vertex buffer view.
		index_buffer.view.BufferLocation = index_buffer.resource->GetGPUVirtualAddress();
		index_buffer.view.Format = Convert(format);
		index_buffer.view.SizeInBytes = static_cast<UINT>(size);


		return handle;
	}

	void DestroyIndexBuffer(Device * device, IndexBufferHandle & handle)
	{
		//Delete handle
		device->m_index_buffer_pool.Free(handle);
	}
	ConstantBufferHandle CreateConstantBuffer(Device * device, const ConstantBufferDesc& constant_buffer_desc)
	{
		ConstantBufferHandle handle = device->m_constant_buffer_pool.Alloc();

		auto& constant_buffer = device->Get(handle);

		CreateResource(device, constant_buffer_desc.init_data, constant_buffer_desc.size, true, constant_buffer.resource);

		//Size needs to be 256byte aligned
		D3D12_CONSTANT_BUFFER_VIEW_DESC dx12_constant_buffer_desc = {};
		dx12_constant_buffer_desc.BufferLocation = constant_buffer.resource->GetGPUVirtualAddress();
		dx12_constant_buffer_desc.SizeInBytes = (constant_buffer_desc.size + 255) & ~255;	// CB size is required to be 256-byte aligned.
		device->m_native_device->CreateConstantBufferView(&dx12_constant_buffer_desc, device->m_constant_buffer_pool.GetDescriptor(handle));

		return handle;

	}
	void DestroyConstantBuffer(Device * device, ConstantBufferHandle & handle)
	{
		//Delete handle
		device->m_constant_buffer_pool.Free(handle);
	}

	UnorderedAccessBufferHandle CreateUnorderedAccessBuffer(Device * device, const UnorderedAccessBufferDesc& unordered_access_buffer_desc)
	{
		UnorderedAccessBufferHandle handle = device->m_unordered_access_buffer_pool.Alloc();

		auto& unordered_access_buffer = device->Get(handle);

		CreateResource(device, unordered_access_buffer_desc.init_data, unordered_access_buffer_desc.element_size * unordered_access_buffer_desc.element_count, true, unordered_access_buffer.resource);

		D3D12_UNORDERED_ACCESS_VIEW_DESC dx12_unordered_access_buffer_desc_desc = {};
		dx12_unordered_access_buffer_desc_desc.Format = DXGI_FORMAT_UNKNOWN;
		dx12_unordered_access_buffer_desc_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		dx12_unordered_access_buffer_desc_desc.Buffer.NumElements = static_cast<UINT>(unordered_access_buffer_desc.element_count);
		dx12_unordered_access_buffer_desc_desc.Buffer.StructureByteStride = static_cast<UINT>(unordered_access_buffer_desc.element_size);
		dx12_unordered_access_buffer_desc_desc.Buffer.FirstElement = 0;
		dx12_unordered_access_buffer_desc_desc.Buffer.CounterOffsetInBytes = 0;
		dx12_unordered_access_buffer_desc_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->m_native_device->CreateUnorderedAccessView(unordered_access_buffer.resource.Get(), nullptr, &dx12_unordered_access_buffer_desc_desc, device->m_unordered_access_buffer_pool.GetDescriptor(handle));

		return handle;
	}
	void DestroyUnorderedAccessBuffer(UnorderedAccessBufferHandle & handle)
	{
	}
}