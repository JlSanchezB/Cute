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

	//Update a resource
	bool UpdateResource(display::Device* device, const SourceResourceData& source_data, const D3D12_RESOURCE_DESC& buffer_desc, ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES resource_state)
	{
		if (buffer_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			//Use the buffer upload path
			display::AllocationUploadBuffer upload_buffer_alloc = display::AllocateUploadBuffer(device, source_data.size);
			
			//Copy to the upload buffer
			memcpy(upload_buffer_alloc.memory, source_data.data, source_data.size);

			//Send to the GPU a copy

			//Open command list
			display::Context* context = OpenCommandList(device, device->m_resource_command_list);
			auto dx12_context = reinterpret_cast<display::DX12Context*>(context);

			dx12_context->command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), resource_state, D3D12_RESOURCE_STATE_COPY_DEST));

			dx12_context->command_list->CopyBufferRegion(resource.Get(), 0, upload_buffer_alloc.resource.Get(), upload_buffer_alloc.offset, source_data.size);

			//Left the resource as it was originally
			dx12_context->command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, resource_state));

			//Close command list
			CloseCommandList(device, context);

			//Execute the command list
			ExecuteCommandList(device, device->m_resource_command_list);
		}
		else
		{
			D3D12MA::ALLOCATION_DESC allocationDesc = {};

			//Upload resource
			ComPtr<ID3D12Resource> upload_resource;
			ComPtr<D3D12MA::Allocation> upload_allocation;

			//Create upload resource
			allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			if (FAILED(device->m_allocator->CreateResource(
				&allocationDesc,
				&CD3DX12_RESOURCE_DESC::Buffer(source_data.size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				&upload_allocation,
				IID_PPV_ARGS(&upload_resource))))
			{
				SetLastErrorMessage(device, "Error creating the copy resource helper in the upload heap");
				return false;
			}
			SetObjectName(upload_resource.Get(), "CopyResource");

			// Copy data to the intermediate upload heap and then schedule a copy 
			// from the upload heap to the default heap
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

			//Left the resource as it was originally
			dx12_context->command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, resource_state));

			//Close command list
			CloseCommandList(device, context);

			//Execute the command list
			ExecuteCommandList(device, device->m_resource_command_list);

			//The upload resource is not needed, add to the deferred resource buffer
			AddDeferredDeleteResource(device, upload_resource, upload_allocation);
		}

		return true;
	}

	//Create a resource
	bool CreateResource(display::Device* device, const SourceResourceData& source_data, const D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_DESC& buffer_desc, ComPtr<ID3D12Resource>& resource, ComPtr<D3D12MA::Allocation>& allocation, display::ResourceMemoryAccess& resource_memory_access, D3D12_RESOURCE_STATES resource_state, D3D12_CLEAR_VALUE* clear_values = nullptr)
	{
		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		
		//Upload resource
		ComPtr<ID3D12Resource> upload_resource;
		ComPtr<D3D12MA::Allocation> upload_allocation;

		allocationDesc.HeapType = heap_type;
		if (FAILED(device->m_allocator->CreateResource(
			&allocationDesc,
			&buffer_desc,
			resource_state,
			clear_values,
			&allocation,
			IID_PPV_ARGS(&resource))))
		{
			SetLastErrorMessage(device, "Error creating a resource in the default heap");
			return false;
		}
		
		if (heap_type == D3D12_HEAP_TYPE_UPLOAD || heap_type == D3D12_HEAP_TYPE_READBACK)
		{
			//capture memory pointer and size
			CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
			if (FAILED(resource->Map(0, &readRange, reinterpret_cast<void**>(&resource_memory_access.memory_data))))
			{
				SetLastErrorMessage(device, "Error mapping to CPU memory a resource");
				return false;
			}
			resource_memory_access.memory_size = source_data.size;
		}

		if (source_data.data)
		{
			if (heap_type == D3D12_HEAP_TYPE_DEFAULT)
			{
				//Create the update resource to update the resource with the default data
				UpdateResource(device, source_data, buffer_desc, resource, resource_state);
			}
			else if (heap_type == D3D12_HEAP_TYPE_UPLOAD && buffer_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
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
	auto CreateRingResources(display::Device* device, const SourceResourceData& source_data, const display::Access access, const D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_DESC& buffer_desc, POOL& pool, D3D12_RESOURCE_STATES resource_state, FUNCTION&& view_create, D3D12_CLEAR_VALUE* clear_values = nullptr)
	{
		//Allocs first resource from the pool
		auto resource_handle = pool.Alloc();
		auto* resource = &device->Get(resource_handle);

		resource->access = access;

		//Create a ring of num frames of resources, starting with the first one
		size_t count = device->m_frame_resources.size();

		POOL::HandleType::WeakHandleVersion resource_handle_ptr = resource_handle;
		while (count)
		{
			//Create a dynamic resource
			if (!CreateResource(device, source_data, heap_type, buffer_desc, resource->resource, resource->allocation, *resource, resource_state, clear_values))
			{
				DeleteRingResource(device, resource_handle, pool);
				return resource_handle;
			}

			//Create views for it
			view_create(device, resource_handle_ptr, *resource);

			if (count > 1)
			{
				//Create next handle in the ring
				device->Get(resource_handle_ptr).next_handle = pool.Alloc();
				resource_handle_ptr = device->Get(resource_handle_ptr).next_handle;
				resource = &device->Get(resource_handle_ptr);

				resource->access = access;
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
	void AddDeferredDeleteResource(display::Device* device, ComPtr<ID3D12Object>& resource, ComPtr<D3D12MA::Allocation>& allocation)
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
		device->m_resource_deferred_delete_ring_buffer.emplace(resource, allocation, device->m_resource_deferred_delete_index);
		//Signal the GPU, so the GPU will update the fence when it reach here
		device->m_command_queue->Signal(device->m_resource_deferred_delete_fence.Get(), device->m_resource_deferred_delete_index);
		//Increase the fence value
		device->m_resource_deferred_delete_index++;
	}

	void AddDeferredDeleteResource(display::Device* device, ComPtr<ID3D12Object>& resource)
	{
		ComPtr<D3D12MA::Allocation> null_allocation;
		AddDeferredDeleteResource(device, resource, null_allocation);
	}

	AllocationUploadBuffer AllocateUploadBuffer(display::Device* device, size_t size)
	{
		AllocationUploadBuffer ret;

		//Align the size to 16
		size = (size + 16) & ~16;

		if (size < device->m_upload_buffer_max_size)
		{
			//Get the active allocation for this thread
			auto& active_allocation = device->m_active_upload_buffers.Get();
			//Can use the active and the pooled buffers
			if (!active_allocation.allocation || active_allocation.current_offset + size >= device->m_upload_buffer_max_size)
			{
				core::MutexGuard pool_access(device->m_update_buffer_pool_mutex);

				//We need to free the current active allocation
				if (active_allocation.allocation)
				{
					//Check if the active is in the current frame
					assert(device->m_upload_buffer_pool[active_allocation.pool_index].frame == device->m_frame_index);
					
					//It just need to get reset
					active_allocation = Device::ActiveUploadBuffer();
				}
				uint64_t last_completed_gpu_frame = display::GetLastCompletedGPUFrame(device);
				
				//Visit the pool for a free slot
				for (size_t i = 0; i < device->m_upload_buffer_pool.size(); ++i)
				{
					auto& upload_buffer_slot = device->m_upload_buffer_pool[i];
					if (upload_buffer_slot.frame <= last_completed_gpu_frame)
					{
						active_allocation.allocation = upload_buffer_slot.allocation.Get();
						active_allocation.current_offset = 0;
						active_allocation.pool_index = i;
						active_allocation.memory_access = upload_buffer_slot.memory_access;
						break;
					}
				}
				if (active_allocation.allocation == nullptr)
				{
					//We need to allocate a new pool resource
					D3D12_RESOURCE_DESC d12_resource_desc = {};
					D3D12_RESOURCE_STATES init_resource_state;

					d12_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(device->m_upload_buffer_max_size);
					init_resource_state = D3D12_RESOURCE_STATE_COMMON;

					ComPtr<D3D12MA::Allocation> upload_allocation;
					ComPtr<ID3D12Resource> upload_resource;
					SourceResourceData no_source_data(nullptr, 0);
					ResourceMemoryAccess resource_memory_access;

					//Create upload resource
					CreateResource(device, no_source_data, D3D12_HEAP_TYPE_UPLOAD, d12_resource_desc, upload_resource, upload_allocation, resource_memory_access, init_resource_state);

					SetObjectName(upload_resource.Get(), "PooledUploadResource");

					//Add buffer into the pool
					device->m_upload_buffer_pool.emplace_back(Device::PooledUploadBuffer{ upload_allocation.Get() , device->m_frame_index , resource_memory_access});

					//Setup active allocation with the last element of the pool
					auto& upload_buffer_slot = device->m_upload_buffer_pool.back();
					active_allocation.allocation = upload_buffer_slot.allocation.Get();
					active_allocation.current_offset = 0;
					active_allocation.pool_index = device->m_upload_buffer_pool.size() - 1;
					active_allocation.memory_access = upload_buffer_slot.memory_access;
				}
			}

			//Just use the current active allocation of this thread
			ret.resource = active_allocation.allocation->GetResource();
			ret.offset = active_allocation.current_offset;
			ret.memory = reinterpret_cast<uint8_t*>(active_allocation.memory_access.memory_data) + active_allocation.current_offset;
			
			//Reserve
			assert(active_allocation.current_offset + size <= device->m_upload_buffer_max_size);
			active_allocation.current_offset += size;
		}
		else
		{
			//Needs to create completly new resource for it and add it to deferred destroy
			D3D12_RESOURCE_DESC d12_resource_desc = {};
			D3D12_RESOURCE_STATES init_resource_state;

			d12_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
			init_resource_state = D3D12_RESOURCE_STATE_COMMON;

			ComPtr<D3D12MA::Allocation> upload_allocation;
			ComPtr<ID3D12Resource> upload_resource;
			SourceResourceData no_source_data(nullptr, 0);
			ResourceMemoryAccess resource_memory_access;

			//Create upload resource
			CreateResource(device, no_source_data, D3D12_HEAP_TYPE_UPLOAD, d12_resource_desc, upload_resource, upload_allocation, resource_memory_access, init_resource_state);

			SetObjectName(upload_resource.Get(), "CopyResource");

			//Return the buffer
			ret.resource = upload_allocation->GetResource();
			ret.offset = 0;
			ret.memory = resource_memory_access.memory_data;

			//Add for deletion when the frame is over
			AddDeferredDeleteResource(device, upload_resource, upload_allocation);
		}
		return ret;
	}

	void UploadBufferReset(display::Device* device)
	{
		//Close all active buffers
		device->m_active_upload_buffers.Visit([](Device::ActiveUploadBuffer& active_upload_buffer)
			{
				active_upload_buffer = Device::ActiveUploadBuffer();
			});
	}

	void DestroyUploadBufferPool(display::Device* device)
	{
		//Clear all active upload buffers
		UploadBufferReset(device);

		device->m_upload_buffer_pool.clear();
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
						[&](WeakBufferHandle resource)
						{
							assert(device->m_buffer_pool[resource].shader_access);
							//Set it as shader resource or constant buffer
							device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle, i),
							device->m_buffer_pool.GetDescriptor(resource, Buffer::kShaderResourceOrConstantBufferDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						},
						[&](AsUAVBuffer resource)
						{
							assert(device->m_buffer_pool[resource].UAV);
							//Set it as unordered access buffer
							device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle, i),
							device->m_buffer_pool.GetDescriptor(resource, Buffer::kShaderUnorderedAccessDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						},
						[&](WeakTexture2DHandle resource)
						{
							//Set it as shader resource or constant buffer
							device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle, i),
							device->m_texture_2d_pool.GetDescriptor(resource, Texture2D::kShaderResourceDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						},
						[&](AsUAVTexture2D resource)
						{
							assert(device->m_texture_2d_pool[resource].UAV);
							//Set it as unordered access buffer
							device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle, i),
							device->m_texture_2d_pool.GetDescriptor(resource, Texture2D::kShaderUnorderedAccessDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						},
						[&](DescriptorTableDesc::NullDescriptor null_descriptor)
						{
							//Nothing to do
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
							[&](WeakBufferHandle resource)
							{
								assert(device->m_buffer_pool[resource].shader_access);
								//Set it as shader resource or constant buffer
								device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle_it, i),
								device->m_buffer_pool.GetDescriptor(GetRingResource(device, resource, frame_index), Buffer::kShaderResourceOrConstantBufferDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							},
							[&](AsUAVBuffer resource)
							{
								assert(device->m_buffer_pool[resource].UAV);
								//Set it as unordered access buffer
								device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle_it, i),
								device->m_buffer_pool.GetDescriptor(GetRingResource(device, WeakBufferHandle(resource), frame_index), Buffer::kShaderUnorderedAccessDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							},
							[&](WeakTexture2DHandle resource)
							{
								//Set it as shader resource or constant buffer
								device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle_it, i),
								device->m_texture_2d_pool.GetDescriptor(GetRingResource(device, resource, frame_index), Texture2D::kShaderResourceDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							},
							[&](AsUAVTexture2D resource)
							{
								assert(device->m_texture_2d_pool[resource].UAV);
								//Set it as unordered access buffer
								device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(handle_it, i),
								device->m_texture_2d_pool.GetDescriptor(GetRingResource(device, WeakTexture2DHandle(resource), frame_index), Texture2D::kShaderUnorderedAccessDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							},
							[&](DescriptorTableDesc::NullDescriptor null_descriptor)
							{
								//Nothing to do
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

	void UpdateDescriptorTable(Device* device, const WeakDescriptorTableHandle& handle, const DescriptorTableDesc::Descritor* descriptor_table, size_t descriptor_count)
	{
		//Get current descriptor
		WeakDescriptorTableHandle current_frame_descriptor_table_handle = GetRingResource(device, handle, device->m_frame_index);

		//Copy descriptors
		for (size_t i = 0; i < descriptor_count; ++i)
		{
			auto& descriptor_table_item = descriptor_table[i];
			std::visit(
				overloaded
				{
					[&](WeakBufferHandle resource)
					{
						assert(device->m_buffer_pool[resource].shader_access);
						//Set it as shader resource or constant buffer
						device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(current_frame_descriptor_table_handle, i),
						device->m_buffer_pool.GetDescriptor(GetRingResource(device, resource, device->m_frame_index), Buffer::kShaderResourceOrConstantBufferDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					},
					[&](AsUAVBuffer resource)
					{
						assert(device->m_buffer_pool[resource].UAV);
						//Set it as unordered access buffer
						device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(current_frame_descriptor_table_handle, i),
						device->m_buffer_pool.GetDescriptor(resource, Buffer::kShaderUnorderedAccessDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					},
					[&](WeakTexture2DHandle resource)
					{
						//Set it as shader resource or constant buffer
						device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(current_frame_descriptor_table_handle, i),
						device->m_texture_2d_pool.GetDescriptor(GetRingResource(device, resource, device->m_frame_index), Texture2D::kShaderResourceDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					},
					[&](AsUAVTexture2D resource)
					{
						assert(device->m_texture_2d_pool[resource].UAV);
						//Set it as unordered access buffer
						device->m_native_device->CopyDescriptorsSimple(1, device->m_descriptor_table_pool.GetDescriptor(current_frame_descriptor_table_handle, i),
						device->m_texture_2d_pool.GetDescriptor(resource, Texture2D::kShaderUnorderedAccessDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					},
					[&](DescriptorTableDesc::NullDescriptor null_descriptor)
					{
						//Nothing to do
					}
				},
				descriptor_table_item);
		}
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

	BufferHandle CreateBuffer(Device* device, const BufferDesc& buffer_desc, const char* name)
	{
		D3D12_RESOURCE_STATES init_resource_state;
		D3D12_RESOURCE_DESC d12_resource_desc = {};

		size_t size = buffer_desc.size;

		if (buffer_desc.type == BufferType::ConstantBuffer)
		{
			//Size needs to be 256bytes multiple
			size = (size + 255) & ~255;	// CB size is required to be 256-byte aligned.
		}

		d12_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
		if (buffer_desc.is_UAV) d12_resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		init_resource_state = D3D12_RESOURCE_STATE_COMMON;

		auto CreateViewsForResource = [&buffer_desc, size, &d12_resource_desc, name](display::Device* device, const WeakBufferHandle& handle, display::Buffer& resource)
		{
			//All resources have resource view
			D3D12_SHADER_RESOURCE_VIEW_DESC dx12_shader_resource_desc = {};
			dx12_shader_resource_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			bool needs_shader_resource_view = true;

			dx12_shader_resource_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			switch (buffer_desc.type)
			{
			case BufferType::VertexBuffer:
			case BufferType::ConstantBuffer:
			case BufferType::IndexBuffer:
				//Nothing to create
				needs_shader_resource_view = false;
				break;
				
			case BufferType::StructuredBuffer:
				dx12_shader_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
				dx12_shader_resource_desc.Buffer.NumElements = static_cast<UINT>(buffer_desc.num_elements);
				dx12_shader_resource_desc.Buffer.StructureByteStride = static_cast<UINT>(buffer_desc.structure_stride);
				break;
			case BufferType::RawAccessBuffer:
				dx12_shader_resource_desc.Format = DXGI_FORMAT_R32_TYPELESS;
				dx12_shader_resource_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
				dx12_shader_resource_desc.Buffer.NumElements = static_cast<UINT>(buffer_desc.size / 4);
				break;
			}

			if (needs_shader_resource_view)
			{
				resource.shader_access = true;
				device->m_native_device->CreateShaderResourceView(resource.resource.Get(), &dx12_shader_resource_desc, device->m_buffer_pool.GetDescriptor(handle, Buffer::kShaderResourceOrConstantBufferDescriptorIndex));
			}

			if (buffer_desc.is_UAV)
			{
				assert(buffer_desc.type != BufferType::ConstantBuffer);
				assert(buffer_desc.type != BufferType::VertexBuffer);

				resource.UAV = true;

				//Create UAV
				D3D12_UNORDERED_ACCESS_VIEW_DESC dx12_unordered_access_buffer_desc_desc = {};
				dx12_unordered_access_buffer_desc_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				dx12_unordered_access_buffer_desc_desc.Buffer.FirstElement = 0;
				dx12_unordered_access_buffer_desc_desc.Buffer.CounterOffsetInBytes = 0;

				switch (buffer_desc.type)
				{
				case BufferType::ConstantBuffer:
				case BufferType::VertexBuffer:
					assert(false);
					break;
				case BufferType::IndexBuffer:
					dx12_unordered_access_buffer_desc_desc.Format = Convert(buffer_desc.format);
					dx12_unordered_access_buffer_desc_desc.Buffer.NumElements = static_cast<UINT>(buffer_desc.num_elements);
					dx12_unordered_access_buffer_desc_desc.Buffer.StructureByteStride = static_cast<UINT>(buffer_desc.structure_stride);
					break;

				case BufferType::StructuredBuffer:
					dx12_unordered_access_buffer_desc_desc.Format = DXGI_FORMAT_UNKNOWN;
					dx12_unordered_access_buffer_desc_desc.Buffer.NumElements = static_cast<UINT>(buffer_desc.num_elements);
					dx12_unordered_access_buffer_desc_desc.Buffer.StructureByteStride = static_cast<UINT>(buffer_desc.structure_stride);
					dx12_unordered_access_buffer_desc_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
					break;
				case BufferType::RawAccessBuffer:
					dx12_unordered_access_buffer_desc_desc.Format = DXGI_FORMAT_R32_TYPELESS;
					dx12_unordered_access_buffer_desc_desc.Buffer.NumElements = static_cast<UINT>(buffer_desc.size / 4);
					dx12_unordered_access_buffer_desc_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
					break;
				}

				device->m_native_device->CreateUnorderedAccessView(resource.resource.Get(), nullptr, &dx12_unordered_access_buffer_desc_desc, device->m_buffer_pool.GetDescriptor(handle, Buffer::kShaderUnorderedAccessDescriptorIndex));
			}

			
			if (buffer_desc.type == BufferType::IndexBuffer)
			{
				// Initialize the vertex buffer view.
				resource.index_buffer_view.BufferLocation = resource.resource->GetGPUVirtualAddress();
				resource.index_buffer_view.Format = Convert(buffer_desc.format);
				resource.index_buffer_view.SizeInBytes = static_cast<UINT>(buffer_desc.size);
			}
			//Access as a vertex buffer
			else if (buffer_desc.type == BufferType::VertexBuffer || buffer_desc.type == BufferType::StructuredBuffer || buffer_desc.type == BufferType::RawAccessBuffer)
			{
				// Initialize the vertex buffer view.
				resource.vertex_buffer_view.BufferLocation = resource.resource->GetGPUVirtualAddress();
				resource.vertex_buffer_view.StrideInBytes = static_cast<UINT>(buffer_desc.structure_stride);
				resource.vertex_buffer_view.SizeInBytes = static_cast<UINT>(buffer_desc.size);
			}
			else if (buffer_desc.type == BufferType::ConstantBuffer)
			{
				D3D12_CONSTANT_BUFFER_VIEW_DESC dx12_constant_buffer_desc = {};
				dx12_constant_buffer_desc.BufferLocation = resource.resource->GetGPUVirtualAddress();
				dx12_constant_buffer_desc.SizeInBytes = static_cast<UINT>(size);
				device->m_native_device->CreateConstantBufferView(&dx12_constant_buffer_desc, device->m_buffer_pool.GetDescriptor(handle, Buffer::kShaderResourceOrConstantBufferDescriptorIndex));

				resource.shader_access = true;
			}

			resource.type = buffer_desc.type;
			resource.name = name;

			SetObjectName(resource.resource.Get(), name);
		};


		if (buffer_desc.access == Access::Static || buffer_desc.access == Access::Upload)
		{
			BufferHandle handle = device->m_buffer_pool.Alloc();
			auto& resource = device->Get(handle);

			resource.access = buffer_desc.access;

			SourceResourceData data(buffer_desc.init_data, buffer_desc.size, 0, 0);

			if (!CreateResource(device, data, (buffer_desc.access == Access::Static) ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD, d12_resource_desc, resource.resource, resource.allocation, resource, init_resource_state))
			{
				device->m_buffer_pool.Free(handle);
				return BufferHandle();
			}

			CreateViewsForResource(device, handle, resource);

			return handle;
		}
		else if (buffer_desc.access == Access::Dynamic || buffer_desc.access == Access::ReadBack)
		{
			SourceResourceData data(buffer_desc.init_data, buffer_desc.size);

			BufferHandle handle = CreateRingResources(device, data, buffer_desc.access, (buffer_desc.access == Access::Dynamic) ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_READBACK,
				d12_resource_desc, device->m_buffer_pool, init_resource_state,
				[&](display::Device* device, const WeakBufferHandle& handle, display::Buffer& resource)
				{
					CreateViewsForResource(device, handle, resource);
				});

			return handle;
		}

		return BufferHandle();
	}

	void DestroyBuffer(Device* device, BufferHandle& handle)
	{
		//Delete handle and linked ones
		DeleteRingResource(device, handle, device->m_buffer_pool);
	}

	Texture2DHandle CreateTexture2D(Device* device, const Texture2DDesc& texture_2d_desc, const char* name)
	{
		D3D12_RESOURCE_STATES init_resource_state;
		D3D12_RESOURCE_DESC d12_resource_desc = {};
		D3D12_CLEAR_VALUE clear_values;
		D3D12_CLEAR_VALUE* clear_values_ptr = nullptr;

		size_t size = texture_2d_desc.size;

		d12_resource_desc.MipLevels = static_cast<UINT>(texture_2d_desc.mips);
		d12_resource_desc.Format = Convert(texture_2d_desc.format);
		d12_resource_desc.Width = static_cast<UINT>(texture_2d_desc.width);
		d12_resource_desc.Height = static_cast<UINT>(texture_2d_desc.height);
		d12_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (texture_2d_desc.is_UAV) d12_resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		d12_resource_desc.DepthOrArraySize = 1;
		d12_resource_desc.SampleDesc.Count = 1;
		d12_resource_desc.SampleDesc.Quality = 0;
		d12_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		if (texture_2d_desc.is_render_target)
		{
			init_resource_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
			d12_resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		} 
		else if (texture_2d_desc.is_depth_buffer)
		{
			init_resource_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			d12_resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			clear_values.DepthStencil = { texture_2d_desc.default_clear, texture_2d_desc.default_stencil };
			clear_values.Format = Convert(texture_2d_desc.format);

			clear_values_ptr = &clear_values;
		}
		else
		{
			init_resource_state = D3D12_RESOURCE_STATE_COMMON;
		}

		auto CreateViewsForResource = [&texture_2d_desc, size, &d12_resource_desc, name](display::Device* device, const WeakTexture2DHandle& handle, display::Texture2D& resource)
		{
			Format read_format = texture_2d_desc.format;
			if (texture_2d_desc.is_depth_buffer && read_format == Format::D32_FLOAT)
			{
				read_format = Format::R32_FLOAT;
			}
			//All resources have resource view
			D3D12_SHADER_RESOURCE_VIEW_DESC dx12_shader_resource_desc = {};
			dx12_shader_resource_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			dx12_shader_resource_desc.Format = Convert(read_format);
			dx12_shader_resource_desc.Texture2D.MipLevels = d12_resource_desc.MipLevels;
			dx12_shader_resource_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			device->m_native_device->CreateShaderResourceView(resource.resource.Get(), &dx12_shader_resource_desc, device->m_texture_2d_pool.GetDescriptor(handle, Texture2D::kShaderResourceDescriptorIndex));

			if (texture_2d_desc.is_UAV)
			{
				resource.UAV = true;

				Format uav_read_format = texture_2d_desc.format;
				if (texture_2d_desc.is_depth_buffer && uav_read_format == Format::D32_FLOAT)
				{
					uav_read_format = Format::R32_FLOAT;
				}

				//Create UAV
				D3D12_UNORDERED_ACCESS_VIEW_DESC dx12_unordered_access_buffer_desc_desc = {};
				dx12_unordered_access_buffer_desc_desc.Format = Convert(uav_read_format);
				dx12_unordered_access_buffer_desc_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

				device->m_native_device->CreateUnorderedAccessView(resource.resource.Get(), nullptr, &dx12_unordered_access_buffer_desc_desc, device->m_texture_2d_pool.GetDescriptor(handle, Texture2D::kShaderUnorderedAccessDescriptorIndex));
			}

			if (texture_2d_desc.is_render_target)
			{
				assert(texture_2d_desc.access == Access::Static);

				resource.RenderTarget = true;
				//Create render target view
				D3D12_RENDER_TARGET_VIEW_DESC dx12_render_target_view_desc = {};
				dx12_render_target_view_desc.Format = Convert(texture_2d_desc.format);
				dx12_render_target_view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				dx12_render_target_view_desc.Texture2D.MipSlice = 0;
				dx12_render_target_view_desc.Texture2D.PlaneSlice = 0;
				device->m_native_device->CreateRenderTargetView(resource.resource.Get(), &dx12_render_target_view_desc, device->m_texture_2d_pool.GetDescriptor(handle, Texture2D::kRenderTargetDescriptorIndex));
			}
			else if (texture_2d_desc.is_depth_buffer)
			{
				assert(texture_2d_desc.access == Access::Static);

				resource.DepthBuffer = true;
				D3D12_DEPTH_STENCIL_VIEW_DESC dx12_depth_stencil_desc = {};
				dx12_depth_stencil_desc.Format = Convert(texture_2d_desc.format);
				dx12_depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dx12_depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;

				device->m_native_device->CreateDepthStencilView(resource.resource.Get(), &dx12_depth_stencil_desc, device->m_texture_2d_pool.GetDescriptor(handle, Texture2D::kDepthBufferDescriptorIndex));
			}
		
			resource.name = name;
			SetObjectName(resource.resource.Get(), name);
		};


		if (texture_2d_desc.access == Access::Static || texture_2d_desc.access == Access::Upload)
		{
			Texture2DHandle handle = device->m_texture_2d_pool.Alloc();
			auto& resource = device->Get(handle);

			SourceResourceData data(texture_2d_desc.init_data, texture_2d_desc.size, texture_2d_desc.pitch, texture_2d_desc.slice_pitch);

			if (!CreateResource(device, data, (texture_2d_desc.access == Access::Static) ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD, d12_resource_desc, resource.resource, resource.allocation, resource, init_resource_state, clear_values_ptr))
			{
				device->m_texture_2d_pool.Free(handle);
				return Texture2DHandle();
			}

			CreateViewsForResource(device, handle, resource);

			return handle;
		}
		else if (texture_2d_desc.access == Access::Dynamic)
		{
			SourceResourceData data(texture_2d_desc.init_data, texture_2d_desc.size);

			Texture2DHandle handle = CreateRingResources(device, data, texture_2d_desc.access, D3D12_HEAP_TYPE_UPLOAD, d12_resource_desc, device->m_texture_2d_pool, init_resource_state,
				[&](display::Device* device, const WeakTexture2DHandle& handle, display::Texture2D& resource)
				{
					CreateViewsForResource(device, handle, resource);
				}, clear_values_ptr);

			return handle;
		}

		return Texture2DHandle();
	}

	void DestroyTexture2D(Device* device, Texture2DHandle& handle)
	{
		//Delete handle and linked ones
		DeleteRingResource(device, handle, device->m_texture_2d_pool);
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
					auto& buffer = device->Get(GetRingResource(device, handle, device->m_frame_index));
					assert(buffer.access == Access::Dynamic);
					memory_data = buffer.memory_data;
					memory_size = buffer.memory_size;
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
					auto& buffer = device->Get(GetRingResource(device, handle, device->m_frame_index));
					assert(buffer.memory_data);
					assert(buffer.access == Access::Dynamic || buffer.access == Access::Upload);
					memory_data = buffer.memory_data;
					memory_size = buffer.memory_size;
				}
			},
			handle);

		//Only dynamic created resources can access the memory
		assert(memory_data);

		return memory_data;
	}

	void* GetLastWrittenResourceMemoryBuffer(Device* device, const ReadBackResourceHandle& handle)
	{
		void* memory_data = nullptr;
		size_t memory_size = 0;

		std::visit(
			overloaded
			{
				[&](auto handle)
				{
					auto& buffer = device->Get(GetRingResource(device, handle, GetLastCompletedGPUFrame(device)));
					assert(buffer.memory_data);
					assert(buffer.access == Access::ReadBack);
					memory_data = buffer.memory_data;
					memory_size = buffer.memory_size;
				}
			},
			handle);

		assert(memory_data);

		return memory_data;
	}
}