#include "display_common.h"
#include "dds_loader.h"

namespace
{
	struct SourceResourceData
	{
		size_t size = 0;
		size_t row_pitch = 0; //Needed for textures
		size_t slice_pitch = 0;
		const void* data;

		//It is a simple buffer
		SourceResourceData(const void* _data, size_t _size) : data(_data), size(_size), row_pitch(_size), slice_pitch(_size)
		{
		}
		//Texture buffer
		SourceResourceData(const void* _data, size_t _size, size_t _row_pitch, size_t _slice_pitch) : data(_data), size(_size), row_pitch(_row_pitch), slice_pitch(_slice_pitch)
		{
		}
	};

	//Create a commited resource
	bool CreateResource(display::Device* device, const SourceResourceData& source_data, bool static_buffer, const D3D12_RESOURCE_DESC& resource_desc, ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES resource_state)
	{
		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;

		if (static_buffer)
		{
			assert(source_data.data); //It is a static resource and this is the only oportunity to copy data

			if (FAILED(device->m_native_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&resource))))
			{
				SetLastErrorMessage(device, "Error creating a resource in the default heap");
				return false;
			}

			if (FAILED(device->m_native_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(source_data.size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&upload_resource))))
			{
				SetLastErrorMessage(device, "Error creating the copy resource helper in the upload heap");
				return false;
			}

		}
		else
		{
			if (FAILED(device->m_native_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&upload_resource))))
			{
				SetLastErrorMessage(device, "Error creating a resource in the upload heap");
				return false;
			}
		}

		if (static_buffer)
		{
			// Copy data to the intermediate upload heap and then schedule a copy 
			// from the upload heap to the vertex buffer.
			D3D12_SUBRESOURCE_DATA copy_data = {};
			copy_data.pData = source_data.data;
			copy_data.RowPitch = source_data.row_pitch;
			copy_data.SlicePitch = source_data.slice_pitch;

			//Command list
			auto& command_list = device->Get(device->m_resource_command_list).resource;

			//Open command list
			display::Context* context = OpenCommandList(device, device->m_resource_command_list);
			auto dx12_context = reinterpret_cast<display::DX12Context*>(context);

			UpdateSubresources<1>(command_list.Get(), resource.Get(), upload_resource.Get(), 0, 0, 1, &copy_data);
			dx12_context->command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, resource_state));

			//Close command list
			CloseCommandList(device, context);

			//Execute the command list
			ExecuteCommandList(device, device->m_resource_command_list);

			//The upload resource is not needed, add to the deferred resource buffer
			AddDeferredDeleteResource(device, upload_resource);
		}
		else
		{
			resource = upload_resource;
			if (source_data.data)
			{
				//We only support here simple buffers
				//Copy to the upload heap
				UINT8* destination_buffer;
				CD3DX12_RANGE read_range(0, 0);		// We do not intend to read from this resource on the CPU.
				if (FAILED(resource->Map(0, &read_range, reinterpret_cast<void**>(&destination_buffer))))
				{
					SetLastErrorMessage(device, "Error mapping to CPU memory a resource");
					return false;
				}
				memcpy(destination_buffer, source_data.data, source_data.size);
				resource->Unmap(0, nullptr);
			}
		}
		return true;
	}

	//Helper function to create a ring resources
	template <typename FUNCTION, typename POOL>
	auto CreateRingResources(display::Device* device, const SourceResourceData& source_data, const D3D12_RESOURCE_DESC& resource_desc, POOL& pool, D3D12_RESOURCE_STATES resource_state, FUNCTION&& view_create)
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
			if (!CreateResource(device, source_data, false, resource_desc, resource->resource, resource_state))
			{
				DeleteRingResource(device, resource_handle, pool);
				return resource_handle;
			}

			//Create views for it
			view_create(device, *resource_handle_ptr, *resource);

			//capture memory pointer and size
			CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
			if (FAILED(resource->resource->Map(0, &readRange, reinterpret_cast<void**>(&resource->memory_data))))
			{
				SetLastErrorMessage(device, "Error mapping to CPU memory a ring resource");
				DeleteRingResource(device, resource_handle, pool);
				return resource_handle;
			}
			resource->memory_size = source_data.size;

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

	VertexBufferHandle CreateVertexBuffer(Device * device, const VertexBufferDesc& vertex_buffer_desc, const char* name)
	{
		SourceResourceData data(vertex_buffer_desc.init_data, vertex_buffer_desc.size);

		if (vertex_buffer_desc.access == Access::Static)
		{
			VertexBufferHandle handle = device->m_vertex_buffer_pool.Alloc();
			auto& vertex_buffer = device->Get(handle);

			if (!CreateResource(device, data, true, CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_desc.size), vertex_buffer.resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
			{
				device->m_vertex_buffer_pool.Free(handle);
				return VertexBufferHandle();
			}		

			// Initialize the vertex buffer view.
			vertex_buffer.view.BufferLocation = vertex_buffer.resource->GetGPUVirtualAddress();
			vertex_buffer.view.StrideInBytes = static_cast<UINT>(vertex_buffer_desc.stride);
			vertex_buffer.view.SizeInBytes = static_cast<UINT>(vertex_buffer_desc.size);

			SetObjectName(vertex_buffer.resource.Get(), name);

			return handle;
		}
		else if (vertex_buffer_desc.access == Access::Dynamic)
		{
			VertexBufferHandle handle = CreateRingResources(device, data, CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_desc.size), device->m_vertex_buffer_pool, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			[&](display::Device* device, const VertexBufferHandle& handle, display::VertexBuffer& vertex_buffer)
			{
				// Initialize the vertex buffer view.
				vertex_buffer.view.BufferLocation = vertex_buffer.resource->GetGPUVirtualAddress();
				vertex_buffer.view.StrideInBytes = static_cast<UINT>(vertex_buffer_desc.stride);
				vertex_buffer.view.SizeInBytes = static_cast<UINT>(vertex_buffer_desc.size);

				SetObjectName(vertex_buffer.resource.Get(), name);
			});

			return handle;
		}
		return VertexBufferHandle();
	}

	void DestroyVertexBuffer(Device * device, VertexBufferHandle & handle)
	{
		//Delete handle
		DeleteRingResource(device, handle, device->m_vertex_buffer_pool);
	}

	IndexBufferHandle CreateIndexBuffer(Device * device, const IndexBufferDesc& index_buffer_desc, const char* name)
	{
		SourceResourceData data(index_buffer_desc.init_data, index_buffer_desc.size);

		if (index_buffer_desc.access == Access::Static)
		{
			IndexBufferHandle handle = device->m_index_buffer_pool.Alloc();

			auto& index_buffer = device->Get(handle);

			if (!CreateResource(device, data, true, CD3DX12_RESOURCE_DESC::Buffer(index_buffer_desc.size), index_buffer.resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
			{
				device->m_index_buffer_pool.Free(handle);
				return IndexBufferHandle();
			}
				

			// Initialize the vertex buffer view.
			index_buffer.view.BufferLocation = index_buffer.resource->GetGPUVirtualAddress();
			index_buffer.view.Format = Convert(index_buffer_desc.format);
			index_buffer.view.SizeInBytes = static_cast<UINT>(index_buffer_desc.size);

			SetObjectName(index_buffer.resource.Get(), name);

			return handle;
		}
		else if (index_buffer_desc.access == Access::Dynamic)
		{
			IndexBufferHandle handle = CreateRingResources(device, data, CD3DX12_RESOURCE_DESC::Buffer(index_buffer_desc.size), device->m_index_buffer_pool, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
				[&](display::Device* device, const IndexBufferHandle& handle, display::IndexBuffer& index_buffer)
			{
				// Initialize the vertex buffer view.
				index_buffer.view.BufferLocation = index_buffer.resource->GetGPUVirtualAddress();
				index_buffer.view.Format = Convert(index_buffer_desc.format);
				index_buffer.view.SizeInBytes = static_cast<UINT>(index_buffer_desc.size);

				SetObjectName(index_buffer.resource.Get(), name);
			});

			return handle;
		}

		return IndexBufferHandle();
	}

	void DestroyIndexBuffer(Device * device, IndexBufferHandle & handle)
	{
		//Delete handle
		DeleteRingResource(device, handle, device->m_index_buffer_pool);
	}
	ConstantBufferHandle CreateConstantBuffer(Device * device, const ConstantBufferDesc& constant_buffer_desc, const char* name)
	{
		size_t size = (constant_buffer_desc.size + 255) & ~255;	// CB size is required to be 256-byte aligned.
		SourceResourceData data(constant_buffer_desc.init_data, size);

		if (constant_buffer_desc.access == Access::Static)
		{
			ConstantBufferHandle handle = device->m_constant_buffer_pool.Alloc();
			auto& constant_buffer = device->Get(handle);

			if (!CreateResource(device, data, true, CD3DX12_RESOURCE_DESC::Buffer(size), constant_buffer.resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
			{
				device->m_constant_buffer_pool.Free(handle);
				return ConstantBufferHandle();
			}

			//Size needs to be 256byte aligned
			D3D12_CONSTANT_BUFFER_VIEW_DESC dx12_constant_buffer_desc = {};
			dx12_constant_buffer_desc.BufferLocation = constant_buffer.resource->GetGPUVirtualAddress();
			dx12_constant_buffer_desc.SizeInBytes = static_cast<UINT>(size);
			device->m_native_device->CreateConstantBufferView(&dx12_constant_buffer_desc, device->m_constant_buffer_pool.GetDescriptor(handle));

			SetObjectName(constant_buffer.resource.Get(), name);

			return handle;
		}
		else if (constant_buffer_desc.access == Access::Dynamic)
		{
			ConstantBufferHandle handle = CreateRingResources(device, data, CD3DX12_RESOURCE_DESC::Buffer(size), device->m_constant_buffer_pool, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
				[&](display::Device* device, const ConstantBufferHandle& handle, display::ConstantBuffer& constant_buffer)
			{
				//Size needs to be 256byte aligned
				D3D12_CONSTANT_BUFFER_VIEW_DESC dx12_constant_buffer_desc = {};
				dx12_constant_buffer_desc.BufferLocation = constant_buffer.resource->GetGPUVirtualAddress();
				dx12_constant_buffer_desc.SizeInBytes = static_cast<UINT>(size);
				device->m_native_device->CreateConstantBufferView(&dx12_constant_buffer_desc, device->m_constant_buffer_pool.GetDescriptor(handle));

				SetObjectName(constant_buffer.resource.Get(), name);
			});


			return handle;
		}	

		return ConstantBufferHandle();
	}
	void DestroyConstantBuffer(Device * device, ConstantBufferHandle & handle)
	{
		//Delete handle and linked ones
		DeleteRingResource(device, handle, device->m_constant_buffer_pool);
	}

	UnorderedAccessBufferHandle CreateUnorderedAccessBuffer(Device * device, const UnorderedAccessBufferDesc& unordered_access_buffer_desc, const char* name)
	{
		size_t size = unordered_access_buffer_desc.element_size * unordered_access_buffer_desc.element_count;

		UnorderedAccessBufferHandle handle = device->m_unordered_access_buffer_pool.Alloc();

		auto& unordered_access_buffer = device->Get(handle);

		if (FAILED(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&unordered_access_buffer.resource))))
		{
			SetLastErrorMessage(device, "Error creating unordered access resource <%s>", name);
			device->m_unordered_access_buffer_pool.Free(handle);
			return UnorderedAccessBufferHandle();
		}

		D3D12_UNORDERED_ACCESS_VIEW_DESC dx12_unordered_access_buffer_desc_desc = {};
		dx12_unordered_access_buffer_desc_desc.Format = DXGI_FORMAT_UNKNOWN;
		dx12_unordered_access_buffer_desc_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		dx12_unordered_access_buffer_desc_desc.Buffer.NumElements = static_cast<UINT>(unordered_access_buffer_desc.element_count);
		dx12_unordered_access_buffer_desc_desc.Buffer.StructureByteStride = static_cast<UINT>(unordered_access_buffer_desc.element_size);
		dx12_unordered_access_buffer_desc_desc.Buffer.FirstElement = 0;
		dx12_unordered_access_buffer_desc_desc.Buffer.CounterOffsetInBytes = 0;
		dx12_unordered_access_buffer_desc_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->m_native_device->CreateUnorderedAccessView(unordered_access_buffer.resource.Get(), nullptr, &dx12_unordered_access_buffer_desc_desc, device->m_unordered_access_buffer_pool.GetDescriptor(handle));

		SetObjectName(unordered_access_buffer.resource.Get(), name);

		return handle;
	}
	void DestroyUnorderedAccessBuffer(Device * device, UnorderedAccessBufferHandle & handle)
	{
		device->m_unordered_access_buffer_pool.Free(handle);
	}

	ShaderResourceHandle CreateShaderResource(Device * device, const ShaderResourceDesc & shader_resource_desc, const char* name)
	{
		D3D12_RESOURCE_STATES init_resource_state;
		D3D12_RESOURCE_DESC d12_resource_desc = {};
		if (shader_resource_desc.type == ShaderResourceType::Texture2D)
		{
			d12_resource_desc.MipLevels = static_cast<UINT>(shader_resource_desc.mips);
			d12_resource_desc.Format = Convert(shader_resource_desc.format);
			d12_resource_desc.Width = static_cast<UINT>(shader_resource_desc.width);
			d12_resource_desc.Height = static_cast<UINT>(shader_resource_desc.height);
			d12_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			d12_resource_desc.DepthOrArraySize = 1;
			d12_resource_desc.SampleDesc.Count = 1;
			d12_resource_desc.SampleDesc.Quality = 0;
			d12_resource_desc.Dimension = Convert<D3D12_RESOURCE_DIMENSION>(shader_resource_desc.type);

			init_resource_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; //Mainly used for pixel shader
		}
		else if (shader_resource_desc.type == ShaderResourceType::Buffer)
		{
			d12_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(shader_resource_desc.size);
			init_resource_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; //Mainly used in compute or vertex shaders
		}

		if (shader_resource_desc.access == Access::Static)
		{
			ShaderResourceHandle handle = device->m_shader_resource_pool.Alloc();
			auto& shader_resource = device->Get(handle);

			SourceResourceData data(shader_resource_desc.init_data, shader_resource_desc.size, shader_resource_desc.pitch, shader_resource_desc.slice_pitch);

			if (!CreateResource(device, data, true, d12_resource_desc, shader_resource.resource, init_resource_state))
			{
				device->m_shader_resource_pool.Free(handle);
				return ShaderResourceHandle();
			}

			//Create view
			D3D12_SHADER_RESOURCE_VIEW_DESC dx12_shader_resource_desc = {};
			dx12_shader_resource_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			dx12_shader_resource_desc.Format = d12_resource_desc.Format;
			dx12_shader_resource_desc.ViewDimension = Convert<D3D12_SRV_DIMENSION>(shader_resource_desc.type);
			if (shader_resource_desc.type == ShaderResourceType::Buffer)
			{
				dx12_shader_resource_desc.Buffer.NumElements = static_cast<UINT>(shader_resource_desc.num_elements);
				dx12_shader_resource_desc.Buffer.StructureByteStride = static_cast<UINT>(shader_resource_desc.structure_stride);
			}
			else if (shader_resource_desc.type == ShaderResourceType::Texture2D)
			{
				dx12_shader_resource_desc.Texture2D.MipLevels = d12_resource_desc.MipLevels;
			}
			device->m_native_device->CreateShaderResourceView(shader_resource.resource.Get(), &dx12_shader_resource_desc, device->m_shader_resource_pool.GetDescriptor(handle));

			SetObjectName(shader_resource.resource.Get(), name);

			return handle;
		}
		else if (shader_resource_desc.access == Access::Dynamic)
		{
			SourceResourceData data(shader_resource_desc.init_data, shader_resource_desc.size);

			if (shader_resource_desc.type == ShaderResourceType::Buffer)
			{
				ShaderResourceHandle handle = CreateRingResources(device, data, d12_resource_desc, device->m_shader_resource_pool, init_resource_state,
					[&](display::Device* device, const ShaderResourceHandle& handle, display::ShaderResource& shader_resource)
				{
					D3D12_SHADER_RESOURCE_VIEW_DESC dx12_shader_resource_desc = {};
					dx12_shader_resource_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					dx12_shader_resource_desc.Format = d12_resource_desc.Format;
					dx12_shader_resource_desc.ViewDimension = Convert<D3D12_SRV_DIMENSION>(shader_resource_desc.type);
					dx12_shader_resource_desc.Buffer.NumElements = static_cast<UINT>(shader_resource_desc.num_elements);
					dx12_shader_resource_desc.Buffer.StructureByteStride = static_cast<UINT>(shader_resource_desc.structure_stride);
			
					device->m_native_device->CreateShaderResourceView(shader_resource.resource.Get(), &dx12_shader_resource_desc, device->m_shader_resource_pool.GetDescriptor(handle));

					SetObjectName(shader_resource.resource.Get(), name);
				});
			}
			else
			{
				SetLastErrorMessage(device, "Dynamic Texture2D SRV are not supported");
				return ShaderResourceHandle();
			}
		}

		return ShaderResourceHandle();
	}

	ShaderResourceHandle CreateTextureResource(Device* device, const void* data, size_t size, const char* name)
	{
		ShaderResourceHandle handle = device->m_shader_resource_pool.Alloc();
		auto& shader_resource = device->Get(handle);

		D3D12_RESOURCE_DESC d12_resource_desc = {};
		std::vector<D3D12_SUBRESOURCE_DATA> sub_resources;
		D3D12_SHADER_RESOURCE_VIEW_DESC dx12_shader_resource_desc = {};

		if (FAILED(dds_loader::CalculateD12Loader(device->m_native_device.Get(), reinterpret_cast<const uint8_t*>(data), size,
			dds_loader::DDS_LOADER_DEFAULT, d12_resource_desc, sub_resources, dx12_shader_resource_desc)))
		{
			device->m_shader_resource_pool.Free(handle);
			SetLastErrorMessage(device, "Error loading the DDS data <%s>", name);
			return ShaderResourceHandle();
		}

		//Create resources
		if (FAILED(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&d12_resource_desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&shader_resource.resource))))
		{
			device->m_shader_resource_pool.Free(handle);
			SetLastErrorMessage(device, "Error creating resource <%s>", name);
			return ShaderResourceHandle();
		}

		//Calculate required size
		UINT64 RequiredSize = 0;
		device->m_native_device->GetCopyableFootprints(&d12_resource_desc, 0, static_cast<UINT>(sub_resources.size()), 0, nullptr, nullptr, nullptr, &RequiredSize);
	
		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;

		//Create upload buffer
		if (FAILED(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(RequiredSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload_resource))))
		{
			device->m_shader_resource_pool.Free(handle);
			SetLastErrorMessage(device, "Error creating upload resource <%s>", name);
			return ShaderResourceHandle();
		}

		//Upload resource
		//Command list
		auto& command_list = device->Get(device->m_resource_command_list).resource;

		display::Context* context = OpenCommandList(device, device->m_resource_command_list);
		auto dx12_context = reinterpret_cast<display::DX12Context*>(context);

		UpdateSubresources<128>(command_list.Get(), shader_resource.resource.Get(), upload_resource.Get(), 0, 0, static_cast<UINT>(sub_resources.size()), &sub_resources[0]);
		dx12_context->command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(shader_resource.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		//Close command list
		CloseCommandList(device, context);

		//Execute the command list
		ExecuteCommandList(device, device->m_resource_command_list);

		//The upload resource is not needed, add to the deferred resource buffer
		AddDeferredDeleteResource(device, upload_resource);

		//Create view
		device->m_native_device->CreateShaderResourceView(shader_resource.resource.Get(), &dx12_shader_resource_desc, device->m_shader_resource_pool.GetDescriptor(handle));

		SetObjectName(shader_resource.resource.Get(), name);

		return handle;
	}

	void DestroyShaderResource(Device * device, ShaderResourceHandle & handle)
	{
		//Delete handle and linked ones
		DeleteRingResource(device, handle, device->m_shader_resource_pool);
	}

	DescriptorTableHandle CreateDescriptorTable(Device * device, const DescriptorTableDesc & descriptor_table_desc)
	{
		DescriptorTableHandle handle = device->m_descriptor_table_pool.Alloc(static_cast<uint16_t>(descriptor_table_desc.num_descriptors));
		auto& descriptor_table = device->Get(handle);

		if (descriptor_table_desc.access == Access::Static)
		{
			//Copy descriptors
			for (size_t i = 0; i < descriptor_table_desc.num_descriptors; ++i)
			{
				auto& descriptor_table_item = descriptor_table_desc.descriptors[i];
				std::visit(
					overloaded
					{
						[&](WeakConstantBufferHandle constant_buffer)
						{
							//We only support static resources, no ring ones
							assert(!device->Get(constant_buffer).next_handle.IsValid());
							device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle, i),
								device->m_constant_buffer_pool.GetDescriptor(constant_buffer), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						},
						[&](WeakUnorderedAccessBufferHandle unordered_access_buffer)
						{
							device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle, i),
								device->m_unordered_access_buffer_pool.GetDescriptor(unordered_access_buffer), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						},
						[&](WeakShaderResourceHandle shader_resource)
						{
							//We only support static resources, no ring ones
							assert(!device->Get(shader_resource).next_handle.IsValid());
							device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle, i),
								device->m_shader_resource_pool.GetDescriptor(shader_resource), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						},
						[&](WeakRenderTargetHandle render_target)
						{
							device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle, i),
							device->m_render_target_pool.GetDescriptor(render_target, 1), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						}
					},
					descriptor_table_item);
			}
			return handle;
		}
		else if (descriptor_table_desc.access == Access::Dynamic)
		{
			//Will create a ring descriptor table, and each descriptor will have the correct frame resource
			size_t count = device->m_frame_resources.size();
			size_t frame_index = 0;
			DescriptorTableHandle* handle_ptr = &handle;
			while (count)
			{
				DescriptorTableHandle& handle_it = *handle_ptr;

				//Copy descriptors
				for (size_t i = 0; i < descriptor_table_desc.num_descriptors; ++i)
				{
					auto& descriptor_table_item = descriptor_table_desc.descriptors[i];
					std::visit(
						overloaded
						{
							[&](WeakConstantBufferHandle constant_buffer)
							{
								device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle_it, i),
									device->m_constant_buffer_pool.GetDescriptor(GetRingResource(device, constant_buffer, frame_index)), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							},
							[&](WeakUnorderedAccessBufferHandle unordered_access_buffer)
							{
								device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle_it, i),
									device->m_unordered_access_buffer_pool.GetDescriptor(unordered_access_buffer), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							},
							[&](WeakShaderResourceHandle shader_resource)
							{
								//We only support static resources
								device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle_it, i),
									device->m_shader_resource_pool.GetDescriptor(GetRingResource(device, shader_resource, frame_index)), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							},
							[&](WeakRenderTargetHandle render_target)
							{
								device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle_it, i),
								device->m_render_target_pool.GetDescriptor(render_target), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							}
						},
					descriptor_table_item);
				}

				if (count > 0)
				{
					//Create next one
					device->Get(handle_it).next_handle = device->m_descriptor_table_pool.Alloc(static_cast<uint16_t>(descriptor_table_desc.num_descriptors));
					handle_ptr = &device->Get(handle_it).next_handle;
				}
				count--;
				frame_index++;
			}
		}
		
		return handle;
	}
	void DestroyDescriptorTable(Device * device, DescriptorTableHandle & handle)
	{
		//Delete handle and linked ones
		DeleteRingResource(device, handle, device->m_descriptor_table_pool);
	}

	SamplerDescriptorTableHandle CreateSamplerDescriptorTable(Device* device, const SamplerDescriptorTableDesc& sampler_descriptor_table)
	{
		//Create a descritpro table with the list of samplers
		SamplerDescriptorTableHandle handle = device->m_sampler_descriptor_table_pool.Alloc(static_cast<uint16_t>(sampler_descriptor_table.num_descriptors));

		for (size_t i = 0; i < sampler_descriptor_table.num_descriptors; ++i)
		{
			D3D12_SAMPLER_DESC dx12_sampler_desc = Convert(sampler_descriptor_table.descriptors[i]);

			device->m_native_device->CreateSampler(&dx12_sampler_desc, device->m_sampler_descriptor_table_pool.GetDescriptor(handle, i));
		}
		return handle;
	}

	void DestroySamplerDescriptorTable(Device * device, SamplerDescriptorTableHandle& handle)
	{
		device->m_sampler_descriptor_table_pool.Free(handle);
	}

	void UpdateResourceBuffer(Device * device, const UpdatableResourceHandle& handle, const void * data, size_t size)
	{
		void* memory_data = nullptr;
		size_t memory_size = 0;

		std::visit(
			overloaded
			{
				[&](auto handle)
				{
					auto& constant_buffer = device->Get(GetRingResource(device, handle, device->m_frame_index));
					memory_data = constant_buffer.memory_data;
					memory_size = constant_buffer.memory_size;
				}
			},
			handle);

		assert(size <= memory_size);
		assert(memory_data);

		//Copy
		if (memory_data)
		{
			memcpy(memory_data, data, size);
			device->uploaded_memory_frame += size;
		}
	}
	void* GetResourceMemoryBuffer(Device* device, const DirectAccessResourceHandle& handle)
	{
		void* memory_data = nullptr;
		size_t memory_size = 0;

		std::visit(
			overloaded
			{
				[&](auto handle)
				{
					auto& constant_buffer = device->Get(GetRingResource(device, handle, device->m_frame_index));
					memory_data = constant_buffer.memory_data;
					memory_size = constant_buffer.memory_size;
				}
			},
			handle);

		assert(memory_data);

		return memory_data;
	}
}