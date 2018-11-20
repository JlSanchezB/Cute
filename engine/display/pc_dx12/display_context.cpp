#include "display_common.h"


namespace display
{
	Device* Context::GetDevice()
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		return dx12_context->device;
	}
	void Context::SetRenderTargets(size_t num_targets, WeakRenderTargetHandle* render_target_array, WeakDepthBufferHandle depth_stencil)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		CD3DX12_CPU_DESCRIPTOR_HANDLE render_target_handles[kMaxNumRenderTargets];

		//Transfert resources to render target and calculate the handles in the render target heap
		for (size_t i = 0; i < num_targets; ++i)
		{
			auto& render_target = device->Get(render_target_array[i]);
			if (render_target.current_state != D3D12_RESOURCE_STATE_RENDER_TARGET)
			{
				command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(render_target.resource.Get(), render_target.current_state, D3D12_RESOURCE_STATE_RENDER_TARGET));
				render_target.current_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
			}

			render_target_handles[i] = device->m_render_target_pool.GetDescriptor(render_target_array[i]);
		}

		if (depth_stencil.IsValid())
		{
			auto& depth_buffer = device->Get(depth_stencil);
			command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depth_buffer.resource.Get(), depth_buffer.current_state, D3D12_RESOURCE_STATE_DEPTH_WRITE));
			depth_buffer.current_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		command_list->OMSetRenderTargets(static_cast<UINT>(num_targets), render_target_handles, FALSE, depth_stencil.IsValid() ? &device->m_depth_buffer_pool.GetDescriptor(depth_stencil) : nullptr);
	}

	void Context::ClearRenderTargetColour(const WeakRenderTargetHandle& render_target_handle, const float colour[4])
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		auto& render_target = device->Get(render_target_handle);

		command_list->ClearRenderTargetView(device->m_render_target_pool.GetDescriptor(render_target_handle), colour, 0, nullptr);
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
	void Context::SetVertexBuffers(size_t start_slot_index, size_t num_vertex_buffers, WeakVertexBufferHandle * vertex_buffer_handles)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		std::array<D3D12_VERTEX_BUFFER_VIEW, 32> vertex_buffer_views;

		for (size_t i = 0; i < num_vertex_buffers; i++)
		{
			vertex_buffer_views[i] = device->Get(GetRingResource(device, vertex_buffer_handles[i], device->m_frame_index)).view;
		}

		command_list->IASetVertexBuffers(static_cast<UINT>(start_slot_index), static_cast<UINT>(num_vertex_buffers), vertex_buffer_views.data());
	}
	void Context::SetIndexBuffer(const WeakIndexBufferHandle & index_buffer_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		auto& index_buffer = device->Get(GetRingResource(device, index_buffer_handle, device->m_frame_index));

		command_list->IASetIndexBuffer(&index_buffer.view);
	}
	void Context::RenderTargetTransition(size_t num_targets, WeakRenderTargetHandle * render_target_array, const ResourceState& dest_state)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;

		std::array< CD3DX12_RESOURCE_BARRIER, kMaxNumRenderTargets> dx12_render_target_transtitions;

		size_t num_transitions = 0;
		for (size_t i = 0; i < num_targets; ++i)
		{
			auto& render_target = device->Get(render_target_array[i]);
			if (render_target.current_state != Convert(dest_state))
			{
				dx12_render_target_transtitions[num_transitions] = CD3DX12_RESOURCE_BARRIER::Transition(render_target.resource.Get(), render_target.current_state, Convert(dest_state));
				render_target.current_state = Convert(dest_state);
				num_transitions++;
			}
		}
		if (num_transitions > 0)
		{
			command_list->ResourceBarrier(static_cast<UINT>(num_transitions), &dx12_render_target_transtitions[0]);
		}

	}
	void Context::SetConstants(const Pipe& pipe, size_t root_parameter, const void * data, size_t num_constants)
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

	void Context::SetConstantBuffer(const Pipe& pipe, size_t root_parameter, const WeakConstantBufferHandle & constant_buffer_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;
		auto& constant_buffer = device->Get(GetRingResource(device, constant_buffer_handle, device->m_frame_index));

		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);

			command_list->SetGraphicsRootConstantBufferView(static_cast<UINT>(root_parameter), constant_buffer.resource->GetGPUVirtualAddress());
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);

			command_list->SetComputeRootConstantBufferView(static_cast<UINT>(root_parameter), constant_buffer.resource->GetGPUVirtualAddress());
		}
	}
	void Context::SetUnorderedAccessBuffer(const Pipe& pipe, size_t root_parameter, const WeakUnorderedAccessBufferHandle & unordered_access_buffer_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;
		auto& unordered_access_buffer = device->Get(unordered_access_buffer_handle);

		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetGraphicsRootUnorderedAccessView(static_cast<UINT>(root_parameter), unordered_access_buffer.resource->GetGPUVirtualAddress());
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetComputeRootUnorderedAccessView(static_cast<UINT>(root_parameter), unordered_access_buffer.resource->GetGPUVirtualAddress());
		}
	}
	void Context::SetShaderResource(const Pipe& pipe, size_t root_parameter, const WeakShaderResourceHandle & shader_resource_handle)
	{
		auto dx12_context = reinterpret_cast<DX12Context*>(this);
		Device* device = dx12_context->device;
		const auto& command_list = dx12_context->command_list;
		auto& shader_resource = device->Get(GetRingResource(device, shader_resource_handle, device->m_frame_index));

		if (pipe == Pipe::Graphics)
		{
			auto& root_signature = device->Get(dx12_context->current_graphics_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetGraphicsRootShaderResourceView(static_cast<UINT>(root_parameter), shader_resource.resource->GetGPUVirtualAddress());
		}
		else if (pipe == Pipe::Compute)
		{
			auto& root_signature = device->Get(dx12_context->current_compute_root_signature);
			assert(root_parameter < root_signature.desc.num_root_parameters);
			command_list->SetComputeRootShaderResourceView(static_cast<UINT>(root_parameter), shader_resource.resource->GetGPUVirtualAddress());
		}
	}

	void Context::SetDescriptorTable(const Pipe& pipe, size_t root_parameter, const WeakDescriptorTableHandle & descriptor_table_handle)
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

	void Context::SetDescriptorTable(const Pipe& pipe, size_t root_parameter, const WeakSamplerDescriptorTableHandle& sampler_descriptor_table_handle)
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
}