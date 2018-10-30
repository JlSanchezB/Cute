#include "display_common.h"

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

		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&vertex_buffer.resource)));

		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;

		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload_resource)));

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the vertex buffer.
		D3D12_SUBRESOURCE_DATA vertex_data = {};
		vertex_data.pData = data;
		vertex_data.RowPitch = size;
		vertex_data.SlicePitch = size;

		//Command list
		auto& command_list = device->Get(device->m_resource_command_list).resource;

		//Open command list
		OpenCommandList(device, device->m_resource_command_list);

		UpdateSubresources<1>(command_list.Get(), vertex_buffer.resource.Get(), upload_resource.Get(), 0, 0, 1, &vertex_data);
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertex_buffer.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Initialize the vertex buffer view.
		vertex_buffer.view.BufferLocation = vertex_buffer.resource->GetGPUVirtualAddress();
		vertex_buffer.view.StrideInBytes = static_cast<UINT>(stride);
		vertex_buffer.view.SizeInBytes = static_cast<UINT>(size);

		//Close command list
		CloseCommandList(device, device->m_resource_command_list);

		//Execute the command list
		ExecuteCommandList(device, device->m_resource_command_list);

		//The upload resource is not needed, add to the deferred resource buffer
		AddDeferredDeleteResource(device, upload_resource);

		return handle;
	}

	void DestroyVertexBuffer(Device * device, VertexBufferHandle & handle)
	{
		//Move the resource to the deferred delete ring buffer
		AddDeferredDeleteResource(device, device->Get(handle).resource);

		//Delete handle
		device->m_vertex_buffer_pool.Free(handle);
	}

	IndexBufferHandle CreateIndexBuffer(Device * device, void* data, size_t size, Format format)
	{
		IndexBufferHandle handle = device->m_index_buffer_pool.Alloc();

		auto& index_buffer = device->Get(handle);

		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&index_buffer.resource)));

		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;

		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload_resource)));

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the vertex buffer.
		D3D12_SUBRESOURCE_DATA index_data = {};
		index_data.pData = data;
		index_data.RowPitch = size;
		index_data.SlicePitch = size;

		//Command list
		auto& command_list = device->Get(device->m_resource_command_list).resource;

		//Open command list
		OpenCommandList(device, device->m_resource_command_list);

		UpdateSubresources<1>(command_list.Get(), index_buffer.resource.Get(), upload_resource.Get(), 0, 0, 1, &index_data);
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(index_buffer.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Initialize the vertex buffer view.
		index_buffer.view.BufferLocation = index_buffer.resource->GetGPUVirtualAddress();
		index_buffer.view.Format = Convert(format);
		index_buffer.view.SizeInBytes = static_cast<UINT>(size);

		//Close command list
		CloseCommandList(device, device->m_resource_command_list);

		//Execute the command list
		ExecuteCommandList(device, device->m_resource_command_list);

		//The upload resource is not needed, add to the deferred resource buffer
		AddDeferredDeleteResource(device, upload_resource);

		return handle;
	}

	void DestroyIndexBuffer(Device * device, IndexBufferHandle & handle)
	{
		//Move the resource to the deferred delete ring buffer
		AddDeferredDeleteResource(device, device->m_index_buffer_pool[handle].resource);

		//Delete handle
		device->m_index_buffer_pool.Free(handle);
	}
	ConstantBufferHandle CreateConstantBuffer(Device * device, void * data, size_t size)
	{
		ConstantBufferHandle handle = device->m_constant_buffer_pool.Alloc();

		auto& constant_buffer = device->Get(handle);

		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&constant_buffer)));

		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;

		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload_resource)));

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the vertex buffer.
		D3D12_SUBRESOURCE_DATA constant_data = {};
		constant_data.pData = data;
		constant_data.RowPitch = size;
		constant_data.SlicePitch = size;

		//Command list
		auto& command_list = device->Get(device->m_resource_command_list).resource;

		//Open command list
		OpenCommandList(device, device->m_resource_command_list);

		UpdateSubresources<1>(command_list.Get(), constant_buffer.Get(), upload_resource.Get(), 0, 0, 1, &constant_data);
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(constant_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		//Close command list
		CloseCommandList(device, device->m_resource_command_list);

		//Execute the command list
		ExecuteCommandList(device, device->m_resource_command_list);

		//The upload resource is not needed, add to the deferred resource buffer
		AddDeferredDeleteResource(device, upload_resource);

		return handle;

	}
	void DestroyConstantBuffer(Device * device, ConstantBufferHandle & handle)
	{
		//Move the resource to the deferred delete ring buffer
		AddDeferredDeleteResource(device, device->Get(handle));

		//Delete handle
		device->m_constant_buffer_pool.Free(handle);
	}

	UnorderedAccessBufferHandle CreateUnorderedAccessBuffer(Device * device, void * data, size_t element_size, size_t num_elements)
	{
		return UnorderedAccessBufferHandle();
	}
	void DestroyUnorderedAccessBuffer(UnorderedAccessBufferHandle & handle)
	{
	}
}