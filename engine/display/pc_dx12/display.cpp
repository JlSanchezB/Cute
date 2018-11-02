#include "display_common.h"


namespace display
{
	_Use_decl_annotations_
		void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
	{
		ComPtr<IDXGIAdapter1> adapter;
		*ppAdapter = nullptr;

		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see if the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}

		*ppAdapter = adapter.Detach();
	}

	// Wait for pending GPU work to complete.
	void WaitForGpu(display::Device* device)
	{
		// Schedule a Signal command in the queue.
		ThrowIfFailed(device->m_command_queue->Signal(device->m_fence.Get(), device->m_frame_resources[device->m_frame_index].fence_value));

		// Wait until the fence has been processed.
		ThrowIfFailed(device->m_fence->SetEventOnCompletion(device->m_frame_resources[device->m_frame_index].fence_value, device->m_fence_event));
		WaitForSingleObjectEx(device->m_fence_event, INFINITE, FALSE);

		// Increment the fence value for the current frame.
		device->m_frame_resources[device->m_frame_index].fence_value++;
	}

	// Prepare to render the next frame.
	void MoveToNextFrame(display::Device* device)
	{
		// Schedule a Signal command in the queue.
		const UINT64 currentFenceValue = device->m_frame_resources[device->m_frame_index].fence_value;
		ThrowIfFailed(device->m_command_queue->Signal(device->m_fence.Get(), currentFenceValue));

		// Update the frame index.
		device->m_frame_index = device->m_swap_chain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is ready.
		if (device->m_fence->GetCompletedValue() < device->m_frame_resources[device->m_frame_index].fence_value)
		{
			ThrowIfFailed(device->m_fence->SetEventOnCompletion(device->m_frame_resources[device->m_frame_index].fence_value, device->m_fence_event));
			WaitForSingleObjectEx(device->m_fence_event, INFINITE, FALSE);
		}

		// Set the fence value for the next frame.
		device->m_frame_resources[device->m_frame_index].fence_value = currentFenceValue + 1;
	}


	void CreateRootSignatureGraphicsState(display::RootSignatureState& root_signature_state, display::GraphicsState& graphics_state, const display::RootSignatureDesc& root_signature)
	{
		//Set all null in the graphics state
		for (auto& constant_buffer : graphics_state.constant_buffers)
		{
			constant_buffer = display::WeakConstantBufferHandle();
		}

		for (auto& unordered_access_buffer : graphics_state.unordered_access_buffers)
		{
			unordered_access_buffer = display::WeakUnorderedAccessBufferHandle();
		}

		for (auto& texture : graphics_state.textures)
		{
			texture = display::WeakTextureHandle();
		}

		//Create root signature state
		root_signature_state.properties.resize(root_signature.num_root_parameters);
		for (auto& property : root_signature_state.properties)
		{
			property.address = 0;
		}
	}
}

//Access to platform::GetHwnd()
namespace platform
{
	extern HWND GetHwnd();
}

namespace display
{
	Device* CreateDevice(const DeviceInitParams& params)
	{
		Device* device = new Device;

		UINT dxgiFactoryFlags = 0;

		// Enable the debug layer (requires the Graphics Tools "optional feature").
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		if (params.debug)
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				// Enable additional debug layers.
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}

		ComPtr<IDXGIFactory4> factory;
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&device->m_native_device)
		));

		// Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(device->m_native_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&device->m_command_queue)));

		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = static_cast<UINT>(params.num_frames);
		swapChainDesc.Width = static_cast<UINT>(params.width);
		swapChainDesc.Height = static_cast<UINT>(params.height);
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swap_chain;
		ThrowIfFailed(factory->CreateSwapChainForHwnd(
			device->m_command_queue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
			platform::GetHwnd(),
			&swapChainDesc,
			nullptr,
			nullptr,
			&swap_chain
		));

		ThrowIfFailed(swap_chain.As(&device->m_swap_chain));
		device->m_frame_index = device->m_swap_chain->GetCurrentBackBufferIndex();

		//Alloc pools
		device->m_render_target_pool.Init(100, 10, params.num_frames, device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		device->m_command_list_pool.Init(500, 10, params.num_frames);
		device->m_root_signature_pool.Init(10, 10, params.num_frames);
		device->m_pipeline_state_pool.Init(2000, 100, params.num_frames);
		device->m_vertex_buffer_pool.Init(2000, 100, params.num_frames);
		device->m_index_buffer_pool.Init(2000, 100, params.num_frames);
		device->m_constant_buffer_pool.Init(2000, 100, params.num_frames);
		device->m_unordered_access_buffer_pool.Init(1000, 10, params.num_frames);
		device->m_texture_pool.Init(2000, 100, params.num_frames);

		//Create frame resources
		device->m_frame_resources.resize(params.num_frames);

		for (size_t i = 0; i < params.num_frames; ++i)
		{
			auto& frame_resource = device->m_frame_resources[i];

			//Alloc handle for the back buffer
			frame_resource.render_target = device->m_render_target_pool.Alloc();

			//Create back buffer for each frame
			{
				auto& render_target = device->m_render_target_pool[frame_resource.render_target];
				render_target.descriptor_handle = device->m_render_target_pool.GetDescriptor(frame_resource.render_target);
				ThrowIfFailed(device->m_swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&render_target.resource)));
				device->m_native_device->CreateRenderTargetView(render_target.resource.Get(), nullptr, render_target.descriptor_handle);
				render_target.current_state = D3D12_RESOURCE_STATE_PRESENT;
			}

			ThrowIfFailed(device->m_native_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame_resource.command_allocator)));
		}

		// Create synchronization objects for deferred delete resources
		{
			ThrowIfFailed(device->m_native_device->CreateFence(device->m_resource_deferred_delete_index, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&device->m_resource_deferred_delete_fence)));
			device->m_resource_deferred_delete_index++;

			device->m_resource_deferred_delete_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (device->m_fence_event == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		//Create command lists
		{
			device->m_present_command_list = CreateCommandList(device);
			device->m_resource_command_list = CreateCommandList(device);
		}

		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		{
			//Create sync fences
			ThrowIfFailed(device->m_native_device->CreateFence(device->m_frame_resources[device->m_frame_index].fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&device->m_fence)));
			device->m_frame_resources[device->m_frame_index].fence_value++;

			// Create an event handle to use for frame synchronization.
			device->m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (device->m_fence_event == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}

			// Wait for the command list to execute; we are reusing the same command 
			// list in our main loop but for now, we just want to wait for setup to 
			// complete before continuing.
			WaitForGpu(device);
		}

	
		return device;
	}

	void DestroyDevice(Device* device)
	{
		// Ensure that the GPU is no longer referencing resources that are about to be
		// cleaned up by the destructor.
		WaitForGpu(device);

		//Destroy deferred delete resources
		DeletePendingResources(device);

		CloseHandle(device->m_fence_event);
		CloseHandle(device->m_resource_deferred_delete_event);

		//Destroy back buffers
		for (auto& frame_resource : device->m_frame_resources)
		{
			device->m_render_target_pool.Free(frame_resource.render_target);
		}

		//Destroy  command lists
		device->m_command_list_pool.Free(device->m_present_command_list);
		device->m_command_list_pool.Free(device->m_resource_command_list);

		//Destroy pools
		device->m_render_target_pool.Destroy();
		device->m_command_list_pool.Destroy();
		device->m_root_signature_pool.Destroy();
		device->m_pipeline_state_pool.Destroy();
		device->m_vertex_buffer_pool.Destroy();
		device->m_index_buffer_pool.Destroy();
		device->m_constant_buffer_pool.Destroy();
		device->m_unordered_access_buffer_pool.Destroy();
		device->m_texture_pool.Destroy();

		delete device;
	}

	//Present
	void Present(Device* device)
	{
		OpenCommandList(device, device->m_present_command_list);

		auto& command_list = device->Get(device->m_present_command_list).resource;

		// Indicate that the back buffer will now be used to present.
		auto& back_buffer = device->Get(device->m_frame_resources[device->m_frame_index].render_target);
		if (back_buffer.current_state != D3D12_RESOURCE_STATE_PRESENT)
		{
			command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(back_buffer.resource.Get(), back_buffer.current_state, D3D12_RESOURCE_STATE_PRESENT));
			back_buffer.current_state = D3D12_RESOURCE_STATE_PRESENT;
		}

		CloseCommandList(device, device->m_present_command_list);

		//Execute command list
		ExecuteCommandList(device, device->m_present_command_list);

		// Present the frame.
		ThrowIfFailed(device->m_swap_chain->Present(1, 0));

		MoveToNextFrame(device);
	}

	//Begin/End Frame
	void BeginFrame(Device* device)
	{
		// Command list allocators can only be reset when the associated 
		// command lists have finished execution on the GPU; apps should use 
		// fences to determine GPU execution progress.
		ThrowIfFailed(GetCommandAllocator(device)->Reset());

		//Delete deferred handles
		device->m_render_target_pool.NextFrame();
		device->m_command_list_pool.NextFrame();
		device->m_root_signature_pool.NextFrame();
		device->m_pipeline_state_pool.NextFrame();
		device->m_vertex_buffer_pool.NextFrame();
		device->m_index_buffer_pool.NextFrame();
		device->m_constant_buffer_pool.NextFrame();
		device->m_unordered_access_buffer_pool.NextFrame();
		device->m_texture_pool.NextFrame();

		//Delete deferred resources
		DeletePendingResources(device);
	}

	void EndFrame(Device* device)
	{

	}

	//Context
	CommandListHandle CreateCommandList(Device* device)
	{
		CommandListHandle handle = device->m_command_list_pool.Alloc();
		ThrowIfFailed(device->m_native_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, GetCommandAllocator(device).Get(), nullptr, IID_PPV_ARGS(&device->Get(handle).resource)));

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		ThrowIfFailed(device->Get(handle).resource->Close());

		return handle;
	}
	void DestroyCommandList(Device* device, CommandListHandle& handle)
	{
		device->m_command_list_pool.Free(handle);
	}

	//Open context, begin recording
	void OpenCommandList(Device* device, const WeakCommandListHandle& handle)
	{
		auto& command_list = device->Get(handle).resource;
		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		ThrowIfFailed(command_list->Reset(GetCommandAllocator(device).Get(), nullptr));

	}
	//Close context, stop recording
	void CloseCommandList(Device* device, const WeakCommandListHandle& handle)
	{
		device->Get(handle).resource->Close();
	}

	void ExecuteCommandList(Device * device, const WeakCommandListHandle& handle)
	{
		auto& command_list = device->Get(handle).resource;

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { command_list.Get()};
		device->m_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	//Get back buffer
	WeakRenderTargetHandle GetBackBuffer(Device* device)
	{
		return device->m_frame_resources[device->m_frame_index].render_target;
	}

	RootSignatureHandle CreateRootSignature(Device * device, const RootSignatureDesc& root_signature_desc)
	{
		RootSignatureHandle handle = device->m_root_signature_pool.Alloc();

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(device->m_native_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_ROOT_PARAMETER1 root_parameters[1];

		D3D12_STATIC_SAMPLER_DESC static_samplers[kMaxNumStaticSamplers] = {};
		for (size_t i = 0; i < root_signature_desc.num_static_samplers; ++i)
		{
			static_samplers[i] = Convert(root_signature_desc.static_samplers[i]);
		}

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(0, root_parameters, static_cast<UINT>(root_signature_desc.num_static_samplers), static_samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(device->m_native_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&device->Get(handle).resource)));
		
		device->Get(handle).desc = root_signature_desc;

		return handle;
	}

	void DestroyRootSignature(Device * device, RootSignatureHandle & root_signature_handle)
	{
		device->m_root_signature_pool.Free(root_signature_handle);
	}

	PipelineStateHandle CreatePipelineState(Device * device, const PipelineStateDesc & pipeline_state_desc)
	{
		PipelineStateHandle handle = device->m_pipeline_state_pool.Alloc();

		//Fill the DX12 structs using our data
		D3D12_GRAPHICS_PIPELINE_STATE_DESC DX12_pipeline_state_desc = {};

		std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements(kMaxNumInputLayoutElements);
		for (size_t i = 0; i < pipeline_state_desc.input_layout.num_elements; i++)
		{
			auto& source_input_element = pipeline_state_desc.input_layout.elements[i];
			input_elements[i].SemanticName = source_input_element.semantic_name;
			input_elements[i].SemanticIndex = static_cast<UINT>(source_input_element.semantic_index);
			input_elements[i].Format = Convert(source_input_element.format);
			input_elements[i].InputSlot = static_cast<UINT>(source_input_element.input_slot);
			input_elements[i].AlignedByteOffset = static_cast<UINT>(source_input_element.aligned_offset);
			input_elements[i].InputSlotClass = Convert(source_input_element.input_type);
			input_elements[i].InstanceDataStepRate = static_cast<UINT>(source_input_element.instance_step_rate);
		}
		DX12_pipeline_state_desc.InputLayout = { input_elements.data(), static_cast<UINT>(pipeline_state_desc.input_layout.num_elements)};

		DX12_pipeline_state_desc.pRootSignature = device->Get(pipeline_state_desc.root_signature).resource.Get();

		DX12_pipeline_state_desc.VS.pShaderBytecode = pipeline_state_desc.vertex_shader.data;
		DX12_pipeline_state_desc.VS.BytecodeLength = pipeline_state_desc.vertex_shader.size;

		DX12_pipeline_state_desc.PS.pShaderBytecode = pipeline_state_desc.pixel_shader.data;
		DX12_pipeline_state_desc.PS.BytecodeLength = pipeline_state_desc.pixel_shader.size;		
		
		D3D12_RASTERIZER_DESC rasterizer_state;
		rasterizer_state.FillMode = Convert(pipeline_state_desc.rasteritation_state.fill_mode);
		rasterizer_state.CullMode = Convert(pipeline_state_desc.rasteritation_state.cull_mode);
		rasterizer_state.FrontCounterClockwise = true;
		rasterizer_state.DepthBias = static_cast<UINT>(pipeline_state_desc.rasteritation_state.depth_bias);
		rasterizer_state.DepthBiasClamp = pipeline_state_desc.rasteritation_state.depth_bias_clamp;
		rasterizer_state.SlopeScaledDepthBias = pipeline_state_desc.rasteritation_state.slope_depth_bias;
		rasterizer_state.DepthClipEnable = pipeline_state_desc.rasteritation_state.depth_clip_enable;
		rasterizer_state.MultisampleEnable = pipeline_state_desc.rasteritation_state.multisample_enable;
		rasterizer_state.AntialiasedLineEnable = false;
		rasterizer_state.ForcedSampleCount = static_cast<UINT>(pipeline_state_desc.rasteritation_state.forced_sample_count);
		rasterizer_state.ConservativeRaster = (pipeline_state_desc.rasteritation_state.convervative_mode) ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;					
		DX12_pipeline_state_desc.RasterizerState = rasterizer_state;

		D3D12_BLEND_DESC blend_desc;
		blend_desc.AlphaToCoverageEnable = pipeline_state_desc.blend_desc.alpha_to_coverage_enable;
		blend_desc.IndependentBlendEnable = pipeline_state_desc.blend_desc.independent_blend_enable;
		for (size_t i = 0; i < kMaxNumRenderTargets; i++)
		{
			auto& dest = blend_desc.RenderTarget[i];
			const auto& source = pipeline_state_desc.blend_desc.render_target_blend[i];
			
			dest.BlendEnable = source.blend_enable;
			dest.LogicOpEnable = false;
			dest.SrcBlend = Convert(source.src_blend);
			dest.DestBlend = Convert(source.dest_blend);
			dest.BlendOp = Convert(source.blend_op);
			dest.SrcBlendAlpha = Convert(source.alpha_src_blend);
			dest.DestBlendAlpha = Convert(source.alpha_dest_blend);
			dest.BlendOpAlpha = Convert(source.alpha_blend_op);
			dest.LogicOp = D3D12_LOGIC_OP_NOOP;
			dest.RenderTargetWriteMask = source.write_mask;
		}
		DX12_pipeline_state_desc.BlendState = blend_desc;

		DX12_pipeline_state_desc.DepthStencilState.DepthEnable = pipeline_state_desc.depth_enable;
		
		DX12_pipeline_state_desc.DepthStencilState.StencilEnable = pipeline_state_desc.stencil_enable;
		
		DX12_pipeline_state_desc.SampleMask = UINT_MAX;
		
		DX12_pipeline_state_desc.PrimitiveTopologyType = Convert(pipeline_state_desc.primitive_topology);

		DX12_pipeline_state_desc.NumRenderTargets = static_cast<UINT>(pipeline_state_desc.num_render_targets);
		for (size_t i = 0; i < kMaxNumRenderTargets; i++)
		{
			if (i < pipeline_state_desc.num_render_targets)
			{
				DX12_pipeline_state_desc.RTVFormats[i] = Convert(pipeline_state_desc.render_target_format[i]);
			}
			else
			{
				DX12_pipeline_state_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
			}
		}
		 
		DX12_pipeline_state_desc.SampleDesc.Count = static_cast<UINT>(pipeline_state_desc.sample_count);

		//Create pipeline state
		ThrowIfFailed(device->m_native_device->CreateGraphicsPipelineState(&DX12_pipeline_state_desc, IID_PPV_ARGS(&device->Get(handle))));

		return handle;
	}

	void DestroyPipelineState(Device * device, PipelineStateHandle & handle)
	{
		device->m_pipeline_state_pool.Free(handle);
	}

	//Context commands
	void SetRenderTargets(Device* device, const WeakCommandListHandle& command_list_handle, size_t num_targets, WeakRenderTargetHandle* render_target_array, WeakRenderTargetHandle * depth_stencil)
	{
		const auto& command_list = device->Get(command_list_handle).resource;

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

			render_target_handles[i] = render_target.descriptor_handle;
		}

		command_list->OMSetRenderTargets(static_cast<UINT>(num_targets), render_target_handles, FALSE, nullptr);
	}

	void ClearRenderTargetColour(Device* device, const WeakCommandListHandle& command_list_handle, const WeakRenderTargetHandle& render_target_handle, const float colour[4])
	{
		auto& render_target = device->Get(render_target_handle);
		auto& command_list = device->Get(command_list_handle).resource;

		command_list->ClearRenderTargetView(render_target.descriptor_handle, colour, 0, nullptr);
	}
	void SetRootSignature(Device * device, const WeakCommandListHandle & command_list_handle, const WeakRootSignatureHandle & root_signature_handle)
	{
		auto& command_list = device->Get(command_list_handle);
		auto& root_signature = device->Get(root_signature_handle);

		command_list.resource->SetGraphicsRootSignature(root_signature.resource.Get());

		//Root signature desc
		command_list.root_signature_desc = root_signature.desc;
		
		//Create states
		CreateRootSignatureGraphicsState(command_list.root_signature_state, command_list.graphics_state, command_list.root_signature_desc);

	}
	void SetPipelineState(Device * device, const WeakCommandListHandle & command_list_handle, const WeakPipelineStateHandle & pipeline_state_handle)
	{
		auto& command_list = device->Get(command_list_handle).resource;
		auto& pipeline_state = device->Get(pipeline_state_handle);

		command_list->SetPipelineState(pipeline_state.Get());
	}
	void SetVertexBuffers(Device * device, const WeakCommandListHandle & command_list_handle, size_t start_slot_index, size_t num_vertex_buffers, WeakVertexBufferHandle * vertex_buffer_handles)
	{
		std::array<D3D12_VERTEX_BUFFER_VIEW, 32> vertex_buffer_views;

		for (size_t i = 0; i < num_vertex_buffers; i++)
		{
			vertex_buffer_views[i] = device->Get(vertex_buffer_handles[i]).view;
		}

		auto& command_list = device->Get(command_list_handle).resource;
		command_list->IASetVertexBuffers(static_cast<UINT>(start_slot_index), static_cast<UINT>(num_vertex_buffers), vertex_buffer_views.data());
	}
	void SetIndexBuffer(Device * device, const WeakCommandListHandle & command_list_handle, const WeakIndexBufferHandle & index_buffer_handle)
	{
		auto& command_list = device->Get(command_list_handle).resource;
		auto& index_buffer = device->Get(index_buffer_handle);

		command_list->IASetIndexBuffer(&index_buffer.view);
	}
	void SetViewport(Device * device, const WeakCommandListHandle & command_list_handle, const Viewport & viewport)
	{
		auto& command_list = device->Get(command_list_handle).resource;

		D3D12_VIEWPORT dx12_viewport;
		dx12_viewport.TopLeftX = viewport.top_left_x;
		dx12_viewport.TopLeftY = viewport.top_left_y;
		dx12_viewport.Width = viewport.width;
		dx12_viewport.Height = viewport.height;
		dx12_viewport.MinDepth = viewport.min_depth;
		dx12_viewport.MaxDepth = viewport.max_depth;

		command_list->RSSetViewports(1, &dx12_viewport);
	}
	void SetScissorRect(Device * device, const WeakCommandListHandle & command_list_handle, const Rect scissor_rect)
	{
		auto& command_list = device->Get(command_list_handle).resource;

		D3D12_RECT dx12_rect;
		dx12_rect.left = static_cast<LONG>(scissor_rect.left);
		dx12_rect.top = static_cast<LONG>(scissor_rect.top);
		dx12_rect.right = static_cast<LONG>(scissor_rect.right);
		dx12_rect.bottom = static_cast<LONG>(scissor_rect.bottom);

		command_list->RSSetScissorRects(1, &dx12_rect);
	}
	void Draw(Device * device, const WeakCommandListHandle & command_list_handle, size_t start_vertex, size_t vertex_count, PrimitiveTopology primitive_topology)
	{
		auto& command_list = device->Get(command_list_handle).resource;
		command_list->IASetPrimitiveTopology(Convert(primitive_topology));

		command_list->DrawInstanced(static_cast<UINT>(vertex_count), 1, static_cast<UINT>(start_vertex), 0);
	}
}