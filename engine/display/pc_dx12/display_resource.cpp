#include "display_common.h"

namespace
{
	//Create a commited resource
	void CreateResource(display::Device* device, void* data, size_t size, bool static_buffer, const D3D12_RESOURCE_DESC& resource_desc, ComPtr<ID3D12Resource>& resource)
	{
		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;

		if (static_buffer)
		{
			ThrowIfFailed(device->m_native_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&resource)));
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
			//Our resource is the upload resource
			resource = upload_resource;

			//Copy to the upload heap
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
			ThrowIfFailed(resource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, data, size);
			resource->Unmap(0, nullptr);
		}
	}

	//Helper function to create a ring resources
	template <typename FUNCTION, typename POOL>
	auto CreateRingResources(display::Device* device,void* data, size_t size, const D3D12_RESOURCE_DESC& resource_desc, POOL& pool, FUNCTION&& view_create)
	{
		//Allocs first resource from the pool
		auto resource_handle = pool.Alloc();
		auto* resource = &device->Get(resource_handle);

		//Create a ring of num frames of resources, starting with the first one
		size_t count = device->m_frame_resources.size();

		auto* resource_handle_ptr = &resource_handle;
		while (count)
		{
			//Create a dynamic resource
			CreateResource(device, data, size, false, resource_desc, resource->resource);

			//Create views for it
			view_create(device, *resource_handle_ptr, *resource);

			if (count > 1)
			{
				//Create next handle in the ring
				(*resource).next_handle = pool.Alloc();
				resource_handle_ptr = &(*resource).next_handle;
				resource = &device->Get(*resource_handle_ptr);
			}
			count--;
		}
	
		return resource_handle;
	}

	//Delete the handle and the ring resource if exist
	template<typename HANDLE, typename POOL>
	void DeleteRingResource(display::Device * device, HANDLE& handle, POOL& pool)
	{
		HANDLE next_handle = std::move(handle);
		while (next_handle.IsValid())
		{
			HANDLE current_handle = std::move(next_handle);
			auto& resource = device->Get(current_handle);
			next_handle = std::move(resource.next_handle);

			//Delete current handle
			pool.Free(current_handle);
		}
	}

	//Return the handle used in the current frame of a ring resource
	template<typename WEAKHANDLE>
	WEAKHANDLE GetCurrentRingResource(display::Device * device, WEAKHANDLE handle)
	{
		//The frame index indicate how many jumps are needed
		size_t count = static_cast<size_t>(device->m_frame_index);

		while (count)
		{
			handle = device->Get(handle).next_handle;
		}

		return handle;
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

	VertexBufferHandle CreateVertexBuffer(Device * device, const VertexBufferDesc& vertex_buffer_desc)
	{
		VertexBufferHandle handle = device->m_vertex_buffer_pool.Alloc();

		auto& vertex_buffer = device->Get(handle);
		
		CreateResource(device, vertex_buffer_desc.init_data, vertex_buffer_desc.size, true, CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_desc.size), vertex_buffer.resource);

		// Initialize the vertex buffer view.
		vertex_buffer.view.BufferLocation = vertex_buffer.resource->GetGPUVirtualAddress();
		vertex_buffer.view.StrideInBytes = static_cast<UINT>(vertex_buffer_desc.stride);
		vertex_buffer.view.SizeInBytes = static_cast<UINT>(vertex_buffer_desc.size);

		return handle;
	}

	void DestroyVertexBuffer(Device * device, VertexBufferHandle & handle)
	{
		//Delete handle
		device->m_vertex_buffer_pool.Free(handle);
	}

	IndexBufferHandle CreateIndexBuffer(Device * device, const IndexBufferDesc& index_buffer_desc)
	{
		IndexBufferHandle handle = device->m_index_buffer_pool.Alloc();

		auto& index_buffer = device->Get(handle);

		CreateResource(device, index_buffer_desc.init_data, index_buffer_desc.size, true, CD3DX12_RESOURCE_DESC::Buffer(index_buffer_desc.size), index_buffer.resource);

		// Initialize the vertex buffer view.
		index_buffer.view.BufferLocation = index_buffer.resource->GetGPUVirtualAddress();
		index_buffer.view.Format = Convert(index_buffer_desc.format);
		index_buffer.view.SizeInBytes = static_cast<UINT>(index_buffer_desc.size);


		return handle;
	}

	void DestroyIndexBuffer(Device * device, IndexBufferHandle & handle)
	{
		//Delete handle
		device->m_index_buffer_pool.Free(handle);
	}
	ConstantBufferHandle CreateConstantBuffer(Device * device, const ConstantBufferDesc& constant_buffer_desc)
	{
		size_t size = (constant_buffer_desc.size + 255) & ~255;	// CB size is required to be 256-byte aligned.

		if (constant_buffer_desc.access == Access::Static)
		{
			ConstantBufferHandle handle = device->m_constant_buffer_pool.Alloc();
			auto& constant_buffer = device->Get(handle);

			CreateResource(device, constant_buffer_desc.init_data, size, true, CD3DX12_RESOURCE_DESC::Buffer(size), constant_buffer.resource);

			//Size needs to be 256byte aligned
			D3D12_CONSTANT_BUFFER_VIEW_DESC dx12_constant_buffer_desc = {};
			dx12_constant_buffer_desc.BufferLocation = constant_buffer.resource->GetGPUVirtualAddress();
			dx12_constant_buffer_desc.SizeInBytes = static_cast<UINT>(size);
			device->m_native_device->CreateConstantBufferView(&dx12_constant_buffer_desc, device->m_constant_buffer_pool.GetDescriptor(handle));

			return handle;
		}
		else if (constant_buffer_desc.access == Access::Dynamic)
		{
			ConstantBufferHandle handle = CreateRingResources(device, constant_buffer_desc.init_data, size, CD3DX12_RESOURCE_DESC::Buffer(size), device->m_constant_buffer_pool,
				[&](display::Device* device, const ConstantBufferHandle& handle, display::Device::ConstantBuffer& constant_buffer)
			{
				//Size needs to be 256byte aligned
				D3D12_CONSTANT_BUFFER_VIEW_DESC dx12_constant_buffer_desc = {};
				dx12_constant_buffer_desc.BufferLocation = constant_buffer.resource->GetGPUVirtualAddress();
				dx12_constant_buffer_desc.SizeInBytes = static_cast<UINT>(size);
				device->m_native_device->CreateConstantBufferView(&dx12_constant_buffer_desc, device->m_constant_buffer_pool.GetDescriptor(handle));
			});
			return handle;
		}	
		else
		{
			return ConstantBufferHandle();
		}
	}
	void DestroyConstantBuffer(Device * device, ConstantBufferHandle & handle)
	{
		//Delete handle and linked ones
		DeleteRingResource(device, handle, device->m_constant_buffer_pool);
	}

	UnorderedAccessBufferHandle CreateUnorderedAccessBuffer(Device * device, const UnorderedAccessBufferDesc& unordered_access_buffer_desc)
	{
		size_t size = unordered_access_buffer_desc.element_size * unordered_access_buffer_desc.element_count;

		UnorderedAccessBufferHandle handle = device->m_unordered_access_buffer_pool.Alloc();

		auto& unordered_access_buffer = device->Get(handle);

		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&unordered_access_buffer.resource)));

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
	void DestroyUnorderedAccessBuffer(Device * device, UnorderedAccessBufferHandle & handle)
	{
		device->m_unordered_access_buffer_pool.Free(handle);
	}

	ShaderResourceHandle CreateShaderResource(Device * device, const ShaderResourceDesc & shader_resource_desc)
	{
		D3D12_RESOURCE_DESC d12_resource_desc = {};
		d12_resource_desc.MipLevels = static_cast<UINT>(shader_resource_desc.mips);
		d12_resource_desc.Format = Convert(shader_resource_desc.format);
		d12_resource_desc.Width = static_cast<UINT>(shader_resource_desc.width);
		d12_resource_desc.Height = static_cast<UINT>(shader_resource_desc.heigth);
		d12_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		d12_resource_desc.DepthOrArraySize = 1;
		d12_resource_desc.SampleDesc.Count = 1;
		d12_resource_desc.SampleDesc.Quality = 0;
		d12_resource_desc.Dimension = ConvertResource(shader_resource_desc.type);

		if (shader_resource_desc.access == Access::Static)
		{
			ShaderResourceHandle handle = device->m_shader_resource_pool.Alloc();
			auto& shader_resource = device->Get(handle);

			CreateResource(device, shader_resource_desc.init_data, shader_resource_desc.size, true, d12_resource_desc, shader_resource.resource);

			//Create view
			D3D12_SHADER_RESOURCE_VIEW_DESC dx12_shader_resource_desc = {};
			dx12_shader_resource_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			dx12_shader_resource_desc.Format = d12_resource_desc.Format;
			dx12_shader_resource_desc.ViewDimension = ConvertView(shader_resource_desc.type);
			dx12_shader_resource_desc.Texture2D.MipLevels = d12_resource_desc.MipLevels;
			device->m_native_device->CreateShaderResourceView(shader_resource.resource.Get(), &dx12_shader_resource_desc, device->m_shader_resource_pool.GetDescriptor(handle));

			return handle;
		}
		else
		{

		}
		return ShaderResourceHandle();
	}
	void DestroyShaderResource(Device * device, ShaderResourceHandle & handle)
	{
		//Delete handle and linked ones
		DeleteRingResource(device, handle, device->m_shader_resource_pool);
	}
}