#include "display.h"
#include "d3dx12.h"

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

		// Synchronization objects.
		UINT m_frame_index;
		HANDLE m_fence_event;
		ComPtr<ID3D12Fence> m_fence;

		//Pool of resources
		struct RenderTarget
		{
			ComPtr<ID3D12Resource> resource;
			CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
			D3D12_RESOURCE_STATES current_state;
		};
		core::HandlePool<CommandListHandle, ComPtr<ID3D12GraphicsCommandList>> m_command_list_pool;
		core::HandlePool<RenderTargetHandle, RenderTarget> m_render_target_pool;
		core::HandlePool<RootSignatureHandle, ComPtr<ID3D12RootSignature>> m_root_signature_pool;
	};
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

		//Create present command list
		{
			device->m_present_command_list = CreateCommandList(device);
		}

		return device;
	}

	void DestroyDevice(Device* device)
	{
		// Ensure that the GPU is no longer referencing resources that are about to be
		// cleaned up by the destructor.
		WaitForGpu(device);

		CloseHandle(device->m_fence_event);

		//Destroy back buffers
		for (auto& frame_resource : device->m_frame_resources)
		{
			device->m_render_target_pool.Free(frame_resource.render_target);
		}

		//Destroy present command list
		device->m_command_list_pool.Free(device->m_present_command_list);

		delete device;
	}

	//Present
	void Present(Device* device)
	{
		OpenCommandList(device, device->m_present_command_list);

		auto& command_list = device->m_command_list_pool[device->m_present_command_list];

		// Indicate that the back buffer will now be used to present.
		auto& back_buffer = device->m_render_target_pool[device->m_frame_resources[device->m_frame_index].render_target];
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
	}

	void EndFrame(Device* device)
	{

	}

	//Context
	CommandListHandle CreateCommandList(Device* device)
	{
		CommandListHandle handle = device->m_command_list_pool.Alloc();
		ThrowIfFailed(device->m_native_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, GetCommandAllocator(device).Get(), nullptr, IID_PPV_ARGS(&device->m_command_list_pool[handle])));

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		ThrowIfFailed(device->m_command_list_pool[handle]->Close());

		return handle;
	}
	void DestroyCommandList(Device* device, CommandListHandle& handle)
	{
		device->m_command_list_pool.Free(handle);
	}

	//Open context, begin recording
	void OpenCommandList(Device* device, const WeakCommandListHandle& handle)
	{
		auto& command_list = device->m_command_list_pool[handle];
		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		ThrowIfFailed(command_list->Reset(GetCommandAllocator(device).Get(), nullptr));

	}
	//Close context, stop recording
	void CloseCommandList(Device* device, const WeakCommandListHandle& handle)
	{
		device->m_command_list_pool[handle]->Close();
	}

	void ExecuteCommandList(Device * device, const WeakCommandListHandle& handle)
	{
		auto& command_list = device->m_command_list_pool[handle];

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { command_list.Get()};
		device->m_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	//Get back buffer
	WeakRenderTargetHandle GetBackBuffer(Device* device)
	{
		return device->m_frame_resources[device->m_frame_index].render_target;
	}

	RootSignatureHandle CreateRootSignature(Device * device, void * data, size_t size)
	{
		RootSignatureHandle handle = device->m_root_signature_pool.Alloc();

		//Create root signature		
		ThrowIfFailed(device->m_native_device->CreateRootSignature(0, data, size, IID_PPV_ARGS(&device->m_root_signature_pool[handle])));
		
		return handle;
	}

	void DestroyRootSignature(Device * device, RootSignatureHandle & root_signature_handle)
	{
		device->m_root_signature_pool.Free(root_signature_handle);
	}

	//Context commands
	void SetRenderTargets(Device* device, const WeakCommandListHandle& command_list_handle, size_t num_targets, WeakRenderTargetHandle* render_target_array, WeakRenderTargetHandle * depth_stencil)
	{
		const auto& command_list = device->m_command_list_pool[command_list_handle];

		CD3DX12_CPU_DESCRIPTOR_HANDLE render_target_handles[8];

		//Transfert resources to render target and calculate the handles in the render target heap
		for (size_t i = 0; i < num_targets; ++i)
		{
			auto& render_target = device->m_render_target_pool[render_target_array[i]];
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
		auto& render_target = device->m_render_target_pool[render_target_handle];
		auto& command_list = device->m_command_list_pool[command_list_handle];

		command_list->ClearRenderTargetView(render_target.descriptor_handle, colour, 0, nullptr);
	}
}