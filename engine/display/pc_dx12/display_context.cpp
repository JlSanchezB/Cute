#include "display_common.h"
#include <vector>
#include <variant>

namespace display
{
	ID3D12GraphicsCommandList* GetCommandListFromDisplayContext(display::Context* context)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(context);

		return dx12_context->command_list.Get();
	}

	Device* Context::GetDevice()
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		return dx12_context->device;
	}

	void Context::SetRenderTargets(uint8_t num_targets, AsRenderTarget* render_target_array, AsDepthBuffer depth_stencil_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		CD3DX12_CPU_DESCRIPTOR_HANDLE render_target_handles[kMaxNumRenderTargets];

		//Transfert resources to render target and calculate the handles in the render target heap
		for (size_t i = 0; i < num_targets; ++i)
		{
			//Render target handle inside the ring buffer (if it exist)
			auto frame_render_target = GetRingResource(device, WeakTexture2DHandle(render_target_array[i]), device->m_frame_index);
			auto& render_target = device->Get(WeakTexture2DHandle(frame_render_target));
			assert(render_target.RenderTarget);

			render_target_handles[i] = device->m_texture_2d_pool.GetDescriptor(frame_render_target, Texture2D::kRenderTargetDescriptorIndex);
		}
		D3D12_CPU_DESCRIPTOR_HANDLE* depth_buffer_descriptor = nullptr;
		if (depth_stencil_handle.IsValid())
		{
			auto& depth_buffer_frame_handle = GetRingResource(device, WeakTexture2DHandle(depth_stencil_handle), device->m_frame_index);
			auto& depth_stencil = device->Get(depth_buffer_frame_handle);
			assert(depth_stencil.DepthBuffer);
			depth_buffer_descriptor = &device->m_texture_2d_pool.GetDescriptor(depth_buffer_frame_handle, Texture2D::kDepthBufferDescriptorIndex);
		}

		command_list->OMSetRenderTargets(static_cast<UINT>(num_targets), render_target_handles, FALSE, depth_buffer_descriptor);
	}

	void Context::ClearRenderTargetColour(const AsRenderTarget& render_target_handle, const float colour[4])
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		auto& frame_render_target_handle = GetRingResource(device, WeakTexture2DHandle(render_target_handle), device->m_frame_index);
		auto& render_target = device->Get(frame_render_target_handle);
		assert(render_target.RenderTarget);

		command_list->ClearRenderTargetView(device->m_texture_2d_pool.GetDescriptor(frame_render_target_handle, Texture2D::kRenderTargetDescriptorIndex), colour, 0, nullptr);
	}

	void Context::ClearDepthStencil(const AsDepthBuffer& depth_stencil_handle, const ClearType& clear_type, std::optional<float> depth, std::optional <uint8_t> stencil)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		auto& frame_depth_stencil_handle = GetRingResource(device, WeakTexture2DHandle(depth_stencil_handle), device->m_frame_index);

		auto& depth_stencil = device->Get(frame_depth_stencil_handle);
		assert(depth_stencil.DepthBuffer);
		D3D12_CLEAR_FLAGS flags;
		if (clear_type == ClearType::Depth)  flags = D3D12_CLEAR_FLAG_DEPTH;
		if (clear_type == ClearType::Stencil)  flags = D3D12_CLEAR_FLAG_STENCIL;
		if (clear_type == ClearType::DepthStencil)  flags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
		command_list->ClearDepthStencilView(device->m_texture_2d_pool.GetDescriptor(frame_depth_stencil_handle, Texture2D::kDepthBufferDescriptorIndex), flags, depth.has_value() ? depth.value() : depth_stencil.default_depth, stencil.has_value() ? stencil.value() : depth_stencil.default_stencil, 0, nullptr);
	}

	void Context::SetRootSignature(const Pipe& pipe, const WeakRootSignatureHandle & root_signature_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		auto& root_signature = device->Get(root_signature_handle);

		if (pipe == Pipe::Graphics)
		{
			command_list->SetGraphicsRootSignature(root_signature.resource.Get());
			dx12_context->current_graphics_root_signature = root_signature_handle;
		}
		else
		{
			command_list->SetComputeRootSignature(root_signature.resource.Get());
			dx12_context->current_compute_root_signature = root_signature_handle;
		}
		
		
	}
	void Context::SetPipelineState(const WeakPipelineStateHandle & pipeline_state_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		auto& pipeline_state = device->Get(pipeline_state_handle);

		command_list->SetPipelineState(pipeline_state.Get());
	}
	
	void Context::SetVertexBuffers(uint8_t start_slot_index, uint8_t num_vertex_buffers, WeakBufferHandle* vertex_buffers_handles)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		std::array<D3D12_VERTEX_BUFFER_VIEW, 32> vertex_buffer_views;

		for (size_t i = 0; i < num_vertex_buffers; i++)
		{
			vertex_buffer_views[i] = device->Get(GetRingResource(device, vertex_buffers_handles[i], device->m_frame_index)).vertex_buffer_view;
		}

		command_list->IASetVertexBuffers(static_cast<UINT>(start_slot_index), static_cast<UINT>(num_vertex_buffers), vertex_buffer_views.data());
	}

	void Context::SetIndexBuffer(const WeakBufferHandle& index_buffer_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		auto& index_buffer = device->Get(GetRingResource(device, index_buffer_handle, device->m_frame_index));

		command_list->IASetIndexBuffer(&index_buffer.index_buffer_view);
	}
	
	void Context::SetConstants(const Pipe& pipe, uint8_t root_parameter, const void * data, size_t num_constants)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);

			command_list->SetGraphicsRoot32BitConstants(static_cast<UINT>(root_parameter), static_cast<UINT>(num_constants), data, 0);
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);

			command_list->SetComputeRoot32BitConstants(static_cast<UINT>(root_parameter), static_cast<UINT>(num_constants), data, 0);
		}
	}

	void Context::SetConstantBuffer(const Pipe& pipe, uint8_t root_parameter, const WeakBufferHandle& constant_buffer_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;
	
		auto& resource = device->Get(GetRingResource(device, constant_buffer_handle, device->m_frame_index));
		assert(resource.ShaderAccess);
		D3D12_GPU_VIRTUAL_ADDRESS gpu_virtual_address = resource.resource->GetGPUVirtualAddress();
		
		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);

			command_list->SetGraphicsRootConstantBufferView(static_cast<UINT>(root_parameter), gpu_virtual_address);
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);

			command_list->SetComputeRootConstantBufferView(static_cast<UINT>(root_parameter), gpu_virtual_address);
		}
	}
	void Context::SetUnorderedAccessBuffer(const Pipe& pipe, uint8_t root_parameter, const WeakBufferHandle& unordered_access_buffer_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;
		auto& resource = device->Get(GetRingResource(device, unordered_access_buffer_handle, device->m_frame_index));
		assert(resource.UAV);
		D3D12_GPU_VIRTUAL_ADDRESS gpu_virtual_address = resource.resource->GetGPUVirtualAddress();
		
		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetGraphicsRootUnorderedAccessView(static_cast<UINT>(root_parameter), gpu_virtual_address);
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetComputeRootUnorderedAccessView(static_cast<UINT>(root_parameter), gpu_virtual_address);
		}
	}
	void Context::SetShaderResource(const Pipe& pipe, uint8_t root_parameter, const WeakBufferHandle& shader_resource_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;
		auto& shader_resource = device->Get(GetRingResource(device, shader_resource_handle, device->m_frame_index));
		assert(shader_resource.ShaderAccess);
		D3D12_GPU_VIRTUAL_ADDRESS gpu_virtual_address = shader_resource.resource->GetGPUVirtualAddress();
		
		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetGraphicsRootShaderResourceView(static_cast<UINT>(root_parameter), gpu_virtual_address);
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetComputeRootShaderResourceView(static_cast<UINT>(root_parameter), gpu_virtual_address);
		}
	}

	void Context::SetDescriptorTable(const Pipe& pipe, uint8_t root_parameter, const WeakDescriptorTableHandle & descriptor_table_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;
		WeakDescriptorTableHandle  current_descriptor_table_handle = GetRingResource(device, descriptor_table_handle, device->m_frame_index);
		auto& descriptor_table = device->Get(current_descriptor_table_handle);

		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetGraphicsRootDescriptorTable(static_cast<UINT>(root_parameter), device->m_descriptor_table_pool.GetGPUDescriptor(current_descriptor_table_handle));
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetComputeRootDescriptorTable(static_cast<UINT>(root_parameter), device->m_descriptor_table_pool.GetGPUDescriptor(current_descriptor_table_handle));
		}
	}

	void Context::SetDescriptorTable(const Pipe& pipe, uint8_t root_parameter, const WeakSamplerDescriptorTableHandle& sampler_descriptor_table_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetGraphicsRootDescriptorTable(static_cast<UINT>(root_parameter), device->m_sampler_descriptor_table_pool.GetGPUDescriptor(sampler_descriptor_table_handle));
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetComputeRootDescriptorTable(static_cast<UINT>(root_parameter), device->m_sampler_descriptor_table_pool.GetGPUDescriptor(sampler_descriptor_table_handle));
		}
	}

	void Context::SetViewport(const Viewport & viewport)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		D3D12_VIEWPORT dx12_viewport;
		dx12_viewport.TopLeftX = viewport.top_left_x;
		dx12_viewport.TopLeftY = viewport.top_left_y;
		dx12_viewport.Width = viewport.width;
		dx12_viewport.Height = viewport.height;
		dx12_viewport.MinDepth = viewport.min_depth;
		dx12_viewport.MaxDepth = viewport.max_depth;

		command_list->RSSetViewports(1, &dx12_viewport);
	}
	void Context::SetScissorRect(const Rect scissor_rect)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		D3D12_RECT dx12_rect;
		dx12_rect.left = static_cast<LONG>(scissor_rect.left);
		dx12_rect.top = static_cast<LONG>(scissor_rect.top);
		dx12_rect.right = static_cast<LONG>(scissor_rect.right);
		dx12_rect.bottom = static_cast<LONG>(scissor_rect.bottom);

		command_list->RSSetScissorRects(1, &dx12_rect);
	}
	void Context::Draw(const DrawDesc& draw_desc)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		command_list->IASetPrimitiveTopology(Convert(draw_desc.primitive_topology));

		command_list->DrawInstanced(static_cast<UINT>(draw_desc.vertex_count), 1, static_cast<UINT>(draw_desc.start_vertex), 0);
	}

	void Context::DrawIndexed(const DrawIndexedDesc& draw_desc)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		command_list->IASetPrimitiveTopology(Convert(draw_desc.primitive_topology));

		command_list->DrawIndexedInstanced(static_cast<UINT>(draw_desc.index_count), 1, static_cast<UINT>(draw_desc.start_index), static_cast<UINT>(draw_desc.base_vertex), 0);
	}

	void Context::DrawIndexedInstanced(const DrawIndexedInstancedDesc& draw_desc)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		command_list->IASetPrimitiveTopology(Convert(draw_desc.primitive_topology));

		command_list->DrawIndexedInstanced(static_cast<UINT>(draw_desc.index_count), static_cast<UINT>(draw_desc.instance_count), static_cast<UINT>(draw_desc.start_index), static_cast<UINT>(draw_desc.base_vertex), static_cast<UINT>(draw_desc.start_instance));
	}

	void Context::IndirectDrawIndexed(const IndirectDrawIndexedDesc& draw_desc)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		command_list->IASetPrimitiveTopology(Convert(draw_desc.primitive_topology));

		command_list->ExecuteIndirect(device->m_indirect_draw_indexed_command_signature.Get(), 1, device->Get(draw_desc.parameters_buffer).resource.Get(), draw_desc.parameters_offset, NULL, 0);
	}

	void Context::IndirectDrawIndexedInstanced(const IndirectDrawIndexedInstancedDesc& draw_desc)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		command_list->IASetPrimitiveTopology(Convert(draw_desc.primitive_topology));

		command_list->ExecuteIndirect(device->m_indirect_draw_indexed_instanced_command_signature.Get(), 1, device->Get(draw_desc.parameters_buffer).resource.Get(), draw_desc.parameters_offset, NULL, 0);
	}
	void Context::ExecuteCompute(const ExecuteComputeDesc& execute_compute_desc)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		const auto& command_list = dx12_context->command_list;

		command_list->Dispatch(execute_compute_desc.group_count_x, execute_compute_desc.group_count_y, execute_compute_desc.group_count_z);
	}
	void Context::AddResourceBarriers(const std::vector<ResourceBarrier>& resource_barriers)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		const size_t num_barriers = resource_barriers.size();
		assert(num_barriers > 0);

		std::unique_ptr<D3D12_RESOURCE_BARRIER[]> dx12_resource_barriers = std::make_unique<D3D12_RESOURCE_BARRIER[]>(num_barriers);

		for (size_t i = 0; i < num_barriers; ++i)
		{
			auto& resource_barrier = resource_barriers[i];

			if (resource_barrier.type == ResourceBarrierType::UnorderAccess)
			{
				std::visit(
					overloaded
					{
						[&](const WeakBufferHandle& handle)
						{
							//Set resource
							auto& resource = device->Get(std::get<WeakBufferHandle>(resource_barrier.resource));
							assert(resource.UAV);

							dx12_resource_barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
							dx12_resource_barriers[i].UAV.pResource = resource.resource.Get();
						},
						[&](const auto& handle)
						{
							assert(false);
						}

					}, resource_barrier.resource);	
			}
			else if (resource_barrier.type == ResourceBarrierType::Transition)
			{
				dx12_resource_barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

				std::visit(
					overloaded
					{
						[&](const WeakBufferHandle& handle)
						{
							auto& resource = device->Get(GetRingResource(device, handle, device->m_frame_index));
							dx12_resource_barriers[i].Transition.pResource = resource.resource.Get();
							resource.current_state = Convert(resource_barrier.state_after);
						},
						[&](const WeakTexture2DHandle& handle)
						{
							auto& resource = device->Get(GetRingResource(device, handle, device->m_frame_index));
							dx12_resource_barriers[i].Transition.pResource = resource.resource.Get();
							resource.current_state = Convert(resource_barrier.state_after);
						}
					}, resource_barrier.resource);
				

				dx12_resource_barriers[i].Transition.StateBefore = Convert(resource_barrier.state_before);
				dx12_resource_barriers[i].Transition.StateAfter = Convert(resource_barrier.state_after);	
			}
		}

		command_list->ResourceBarrier(static_cast<UINT>(num_barriers), dx12_resource_barriers.get());
	}
}