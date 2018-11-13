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
	void CreateResource(display::Device* device, const SourceResourceData& source_data, bool static_buffer, const D3D12_RESOURCE_DESC& resource_desc, ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES resource_state)
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

			ThrowIfFailed(device->m_native_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(source_data.size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&upload_resource)));

		}
		else
		{
			ThrowIfFailed(device->m_native_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&upload_resource)));
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
			OpenCommandList(device, device->m_resource_command_list);

			UpdateSubresources<1>(command_list.Get(), resource.Get(), upload_resource.Get(), 0, 0, 1, &copy_data);
			command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, resource_state));

			//Close command list
			CloseCommandList(device, device->m_resource_command_list);

			//Execute the command list
			ExecuteCommandList(device, device->m_resource_command_list);

			//The upload resource is not needed, add to the deferred resource buffer
			AddDeferredDeleteResource(device, upload_resource);
		}
		else
		{
			//Our resource is the upload resource, we only support simple linear ones, no textures
			resource = upload_resource;

			//Copy to the upload heap
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
			ThrowIfFailed(resource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, source_data.data, source_data.size);
			resource->Unmap(0, nullptr);
		}
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
			CreateResource(device, source_data, false, resource_desc, resource->resource, resource_state);

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
		VertexBufferHandle handle = device->m_vertex_buffer_pool.Alloc();

		auto& vertex_buffer = device->Get(handle);
		
		SourceResourceData data(vertex_buffer_desc.init_data, vertex_buffer_desc.size);

		CreateResource(device, data, true, CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_desc.size), vertex_buffer.resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		// Initialize the vertex buffer view.
		vertex_buffer.view.BufferLocation = vertex_buffer.resource->GetGPUVirtualAddress();
		vertex_buffer.view.StrideInBytes = static_cast<UINT>(vertex_buffer_desc.stride);
		vertex_buffer.view.SizeInBytes = static_cast<UINT>(vertex_buffer_desc.size);

		SetObjectName(vertex_buffer.resource.Get(), name);

		return handle;
	}

	void DestroyVertexBuffer(Device * device, VertexBufferHandle & handle)
	{
		//Delete handle
		device->m_vertex_buffer_pool.Free(handle);
	}

	IndexBufferHandle CreateIndexBuffer(Device * device, const IndexBufferDesc& index_buffer_desc, const char* name)
	{
		IndexBufferHandle handle = device->m_index_buffer_pool.Alloc();

		auto& index_buffer = device->Get(handle);

		SourceResourceData data(index_buffer_desc.init_data, index_buffer_desc.size);

		CreateResource(device, data, true, CD3DX12_RESOURCE_DESC::Buffer(index_buffer_desc.size), index_buffer.resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		// Initialize the vertex buffer view.
		index_buffer.view.BufferLocation = index_buffer.resource->GetGPUVirtualAddress();
		index_buffer.view.Format = Convert(index_buffer_desc.format);
		index_buffer.view.SizeInBytes = static_cast<UINT>(index_buffer_desc.size);

		SetObjectName(index_buffer.resource.Get(), name);

		return handle;
	}

	void DestroyIndexBuffer(Device * device, IndexBufferHandle & handle)
	{
		//Delete handle
		device->m_index_buffer_pool.Free(handle);
	}
	ConstantBufferHandle CreateConstantBuffer(Device * device, const ConstantBufferDesc& constant_buffer_desc, const char* name)
	{
		size_t size = (constant_buffer_desc.size + 255) & ~255;	// CB size is required to be 256-byte aligned.
		SourceResourceData data(constant_buffer_desc.init_data, size);

		if (constant_buffer_desc.access == Access::Static)
		{
			ConstantBufferHandle handle = device->m_constant_buffer_pool.Alloc();
			auto& constant_buffer = device->Get(handle);

			CreateResource(device, data, true, CD3DX12_RESOURCE_DESC::Buffer(size), constant_buffer.resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

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
				[&](display::Device* device, const ConstantBufferHandle& handle, display::Device::ConstantBuffer& constant_buffer)
			{
				//Size needs to be 256byte aligned
				D3D12_CONSTANT_BUFFER_VIEW_DESC dx12_constant_buffer_desc = {};
				dx12_constant_buffer_desc.BufferLocation = constant_buffer.resource->GetGPUVirtualAddress();
				dx12_constant_buffer_desc.SizeInBytes = static_cast<UINT>(size);
				device->m_native_device->CreateConstantBufferView(&dx12_constant_buffer_desc, device->m_constant_buffer_pool.GetDescriptor(handle));

				//Get the pointer to the memory
				CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
				ThrowIfFailed(constant_buffer.resource->Map(0, &readRange, reinterpret_cast<void**>(&constant_buffer.memory_data)));
				constant_buffer.memory_size = size;

				SetObjectName(constant_buffer.resource.Get(), name);
			});


			return handle;
		}	
		else
		{
			std::runtime_error("Discard constant buffers are not supported");
			return ConstantBufferHandle();
		}
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

		SetObjectName(unordered_access_buffer.resource.Get(), name);

		return handle;
	}
	void DestroyUnorderedAccessBuffer(Device * device, UnorderedAccessBufferHandle & handle)
	{
		device->m_unordered_access_buffer_pool.Free(handle);
	}

	ShaderResourceHandle CreateShaderResource(Device * device, const ShaderResourceDesc & shader_resource_desc, const char* name)
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
		d12_resource_desc.Dimension = Convert<D3D12_RESOURCE_DIMENSION>(shader_resource_desc.type);

		if (shader_resource_desc.access == Access::Static)
		{
			ShaderResourceHandle handle = device->m_shader_resource_pool.Alloc();
			auto& shader_resource = device->Get(handle);

			SourceResourceData data(shader_resource_desc.init_data, shader_resource_desc.size, shader_resource_desc.pitch, shader_resource_desc.slice_pitch);

			CreateResource(device, data, true, d12_resource_desc, shader_resource.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			//Create view
			D3D12_SHADER_RESOURCE_VIEW_DESC dx12_shader_resource_desc = {};
			dx12_shader_resource_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			dx12_shader_resource_desc.Format = d12_resource_desc.Format;
			dx12_shader_resource_desc.ViewDimension = Convert<D3D12_SRV_DIMENSION>(shader_resource_desc.type);
			dx12_shader_resource_desc.Texture2D.MipLevels = d12_resource_desc.MipLevels;
			device->m_native_device->CreateShaderResourceView(shader_resource.resource.Get(), &dx12_shader_resource_desc, device->m_shader_resource_pool.GetDescriptor(handle));

			SetObjectName(shader_resource.resource.Get(), name);

			return handle;
		}
		else
		{
			std::runtime_error("Only static shader resources are supported");
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

		ThrowIfFailed(dds_loader::CalculateD12Loader(device->m_native_device.Get(), reinterpret_cast<const uint8_t*>(data), size,
			dds_loader::DDS_LOADER_DEFAULT, d12_resource_desc, sub_resources, dx12_shader_resource_desc));

		//Create resources
		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&d12_resource_desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&shader_resource.resource)));

		//Calculate required size
		UINT64 RequiredSize = 0;
		device->m_native_device->GetCopyableFootprints(&d12_resource_desc, 0, static_cast<UINT>(sub_resources.size()), 0, nullptr, nullptr, nullptr, &RequiredSize);
	
		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;

		//Create upload buffer
		ThrowIfFailed(device->m_native_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(RequiredSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload_resource)));

		//Upload resource
		//Command list
		auto& command_list = device->Get(device->m_resource_command_list).resource;

		//Open command list
		OpenCommandList(device, device->m_resource_command_list);

		UpdateSubresources<128>(command_list.Get(), shader_resource.resource.Get(), upload_resource.Get(), 0, 0, static_cast<UINT>(sub_resources.size()), &sub_resources[0]);
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(shader_resource.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		//Close command list
		CloseCommandList(device, device->m_resource_command_list);

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

	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

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
						[&](WeakSamplerDescriptorTableHandle sampler)
						{
							assert(false); //Not supported
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
							[&](WeakSamplerDescriptorTableHandle sampler)
							{
								assert(false); //Not supported
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
		else
		{
			std::runtime_error("Only static and dynamic descriptor tables are supported");
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

	void UpdateConstantBuffer(Device * device, const WeakConstantBufferHandle & constant_buffer_handle, const void * data, size_t size)
	{
		auto& constant_buffer = device->Get(GetRingResource(device, constant_buffer_handle, device->m_frame_index));
		assert(size <= constant_buffer.memory_size);

		//Copy
		memcpy(constant_buffer.memory_data, data, size);
	}
}