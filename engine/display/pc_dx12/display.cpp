#include <display\display.h>
#include "d3dx12.h"
#include "display_convert.h"
#include <core/ring_buffer.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <wrl.h>
#include <shellapi.h>

//TODO: Added tearing fullscreen mode

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
using Microsoft::WRL::ComPtr;

namespace display
{
	//State of the properties to be send to the GPU
	struct State
	{
		static const size_t kNumMaxProperties = 16;

		std::array<D3D12_GPU_VIRTUAL_ADDRESS, kNumMaxProperties> constant_buffers;
		std::array<D3D12_GPU_VIRTUAL_ADDRESS, kNumMaxProperties> unordered_access_buffers;
		std::array<D3D12_GPU_VIRTUAL_ADDRESS, kNumMaxProperties> textures;
	};

	//State of the properties that has been set in the root signature
	struct RootSignatureState
	{

	};

	//Device internal implementation
	struct Device
	{
		//DX12 device
		ComPtr<ID3D12Device> m_native_device;

		//Device resources

		//Per frame system resources
		struct FrameResources
		{
			ComPtr<ID3D12CommandAllocator> command_allocator;
			UINT64 fence_value;
			RenderTargetHandle render_target;
		};
		std::vector< FrameResources> m_frame_resources;
		
		//Heaps
		const static size_t kRenderTargetHeapSize = 100;
		ComPtr<ID3D12DescriptorHeap> m_render_target_heap;
		UINT m_render_target_descriptor_size;

		//Global resources
		ComPtr<ID3D12CommandQueue> m_command_queue;
		ComPtr<IDXGISwapChain3> m_swap_chain;
		CommandListHandle m_present_command_list;
		CommandListHandle m_resource_command_list;

		// Synchronization objects.
		UINT m_frame_index;
		HANDLE m_fence_event;
		ComPtr<ID3D12Fence> m_fence;

		//Pool of resources
		struct CommandList
		{
			ComPtr<ID3D12GraphicsCommandList> resource;
			State state;
		};
		struct RootSignature
		{
			ComPtr<ID3D12RootSignature> resource;
			RootSignatureDesc desc;
		};
		using PipelineState = ComPtr<ID3D12PipelineState>;
		struct RenderTarget
		{
			ComPtr<ID3D12Resource> resource;
			CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
			D3D12_RESOURCE_STATES current_state;
		};
		struct VertexBuffer
		{
			ComPtr<ID3D12Resource> resource;
			D3D12_VERTEX_BUFFER_VIEW view;
		};
		struct IndexBuffer
		{
			ComPtr<ID3D12Resource> resource;
			D3D12_INDEX_BUFFER_VIEW view;
		};
		using ConstantBuffer = ComPtr<ID3D12Resource>;
		using UnorderedAccessBuffer = ComPtr<ID3D12Resource>;
		using TextureBuffer = ComPtr<ID3D12Resource>;

		core::HandlePool<CommandListHandle, CommandList> m_command_list_pool;
		core::HandlePool<RenderTargetHandle, RenderTarget> m_render_target_pool;
		core::HandlePool<RootSignatureHandle, RootSignature> m_root_signature_pool;
		core::HandlePool<PipelineStateHandle, PipelineState> m_pipeline_state_pool;
		core::HandlePool<VertexBufferHandle, VertexBuffer> m_vertex_buffer_pool;
		core::HandlePool<IndexBufferHandle, IndexBuffer> m_index_buffer_pool;
		core::HandlePool<ConstantBufferHandle, ConstantBuffer> m_constant_buffer_pool;
		core::HandlePool<UnorderedAccessBufferHandle, UnorderedAccessBuffer> m_unordered_access_buffer_pool;
		core::HandlePool<TextureHandle, TextureBuffer> m_texture_pool;

		//Accesor to the resources (we need a specialitation for each type)
		template<typename HANDLE>
		auto& Get(const HANDLE&);

		//Deferred delete resource ring buffer
		struct DeferredResourceDelete
		{
			//Resource to delete
			ComPtr<ID3D12Object> resource;
			//Value signal to the fence
			UINT64 fence_value;

			DeferredResourceDelete()
			{
			}

			DeferredResourceDelete(ComPtr<ID3D12Object>& _resource, UINT64 _fence_value) : resource(_resource), fence_value(_fence_value)
			{
			}
		};
		core::RingBuffer<DeferredResourceDelete, 1000> m_resource_deferred_delete_ring_buffer;
		
		//This fence represent the index of the resource to be deleted
		//Each time a resource needs to be deleted, it will get added into the ring with the current fence value
		//The resource needs to be alive until the GPU update the fence value
		ComPtr<ID3D12Fence> m_resource_deferred_delete_fence;

		//Event in case we need to wait for the GPU if the ring buffer is full
		HANDLE m_resource_deferred_delete_event;

		//Current CPU fence value
		UINT64 m_resource_deferred_delete_index = 1;
	};

	template<>
	inline auto& Device::Get<CommandListHandle>(const CommandListHandle& handle)
	{
		return this->m_command_list_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakCommandListHandle>(const WeakCommandListHandle& handle)
	{
		return this->m_command_list_pool[handle];
	}

	template<>
	inline auto& Device::Get<RenderTargetHandle>(const RenderTargetHandle& handle)
	{
		return this->m_render_target_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakRenderTargetHandle>(const WeakRenderTargetHandle& handle)
	{
		return this->m_render_target_pool[handle];
	}

	template<>
	inline auto& Device::Get<RootSignatureHandle>(const RootSignatureHandle& handle)
	{
		return this->m_root_signature_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakRootSignatureHandle>(const WeakRootSignatureHandle& handle)
	{
		return this->m_root_signature_pool[handle];
	}

	template<>
	inline auto& Device::Get<PipelineStateHandle>(const PipelineStateHandle& handle)
	{
		return this->m_pipeline_state_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakPipelineStateHandle>(const WeakPipelineStateHandle& handle)
	{
		return this->m_pipeline_state_pool[handle];
	}

	template<>
	inline auto& Device::Get<VertexBufferHandle>(const VertexBufferHandle& handle)
	{
		return this->m_vertex_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakVertexBufferHandle>(const WeakVertexBufferHandle& handle)
	{
		return this->m_vertex_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<IndexBufferHandle>(const IndexBufferHandle& handle)
	{
		return this->m_index_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakIndexBufferHandle>(const WeakIndexBufferHandle& handle)
	{
		return this->m_index_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<ConstantBufferHandle>(const ConstantBufferHandle& handle)
	{
		return this->m_constant_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakConstantBufferHandle>(const WeakConstantBufferHandle& handle)
	{
		return this->m_constant_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<UnorderedAccessBufferHandle>(const UnorderedAccessBufferHandle& handle)
	{
		return this->m_unordered_access_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakUnorderedAccessBufferHandle>(const WeakUnorderedAccessBufferHandle& handle)
	{
		return this->m_unordered_access_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<TextureHandle>(const TextureHandle& handle)
	{
		return this->m_texture_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakTextureHandle>(const WeakTextureHandle& handle)
	{
		return this->m_texture_pool[handle];
	}
}

namespace
{
	inline std::string HrToString(HRESULT hr)
	{
		char s_str[64] = {};
		sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
		return std::string(s_str);
	}

	class HrException : public std::runtime_error
	{
	public:
		HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
		HRESULT Error() const { return m_hr; }
	private:
		const HRESULT m_hr;
	};

	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw HrException(hr);
		}
	}
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

	inline ComPtr<ID3D12CommandAllocator>& GetCommandAllocator(display::Device* device)
	{
		return device->m_frame_resources[device->m_frame_index].command_allocator;
	}

	//Delete resources that are not needed by the GPU
	size_t DeletePendingResources(display::Device* device)
	{
		if (device->m_resource_deferred_delete_ring_buffer.empty())
		{
			return 0;
		}

		size_t count = 0;
		for(; !device->m_resource_deferred_delete_ring_buffer.empty();)
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
	template <typename RESOURCE>
	void AddDeferredDeleteResource(display::Device* device, RESOURCE&& resource)
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

		ComPtr<ID3D12Object> object;
		ThrowIfFailed(resource.As(&object));

		//Add the resource to the ring buffer
		device->m_resource_deferred_delete_ring_buffer.emplace(object, device->m_resource_deferred_delete_index);
		//Signal the GPU, so the GPU will update the fence when it reach here
		device->m_command_queue->Signal(device->m_resource_deferred_delete_fence.Get(), device->m_resource_deferred_delete_index);
		//Increase the fence value
		device->m_resource_deferred_delete_index++;
	}
}

#define SAFE_RELEASE(p) if (p) (p)->Release()

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

		//Alloc pools
		device->m_render_target_pool.Init(Device::kRenderTargetHeapSize, 10);
		device->m_command_list_pool.Init(500, 10);
		device->m_root_signature_pool.Init(10, 10);
		device->m_pipeline_state_pool.Init(2000, 100);
		device->m_vertex_buffer_pool.Init(2000, 100);
		device->m_index_buffer_pool.Init(2000, 100);
		device->m_constant_buffer_pool.Init(2000, 100);
		device->m_unordered_access_buffer_pool.Init(1000, 10);
		device->m_texture_pool.Init(2000, 100);

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

		// Create descriptor heaps.
		{
			// Describe and create a render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = static_cast<UINT>(Device::kRenderTargetHeapSize);
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(device->m_native_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&device->m_render_target_heap)));

			device->m_render_target_descriptor_size = device->m_native_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		//Create frame resources
		device->m_frame_resources.resize(params.num_frames);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(device->m_render_target_heap->GetCPUDescriptorHandleForHeapStart());

		for (size_t i = 0; i < params.num_frames; ++i)
		{
			auto& frame_resource = device->m_frame_resources[i];

			//Alloc handle for the back buffer
			frame_resource.render_target = device->m_render_target_pool.Alloc();

			//Create back buffer for each frame
			{
				auto& render_target = device->m_render_target_pool[frame_resource.render_target];

				ThrowIfFailed(device->m_swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&render_target.resource)));
				device->m_native_device->CreateRenderTargetView(render_target.resource.Get(), nullptr, rtv_handle);
				render_target.descriptor_handle = rtv_handle;
				render_target.current_state = D3D12_RESOURCE_STATE_PRESENT;

				rtv_handle.Offset(1, device->m_render_target_descriptor_size);
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
		//Move the resource to the deferred delete ring buffer
		AddDeferredDeleteResource(device, device->Get(handle).resource);

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
		//Move the resource to the deferred delete ring buffer
		AddDeferredDeleteResource(device, device->Get(root_signature_handle).resource);

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
		//Move the resource to the deferred delete ring buffer
		AddDeferredDeleteResource(device, device->Get(handle));

		device->m_pipeline_state_pool.Free(handle);
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