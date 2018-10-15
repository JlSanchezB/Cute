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
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<IDXGISwapChain3> m_swapChain;
		std::vector<ComPtr<ID3D12Resource>> m_renderTargets;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		UINT m_rtvDescriptorSize;

		// Synchronization objects.
		UINT m_frameIndex;
		HANDLE m_fenceEvent;
		ComPtr<ID3D12Fence> m_fence;
		std::vector <UINT64> m_fenceValues;
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
		ThrowIfFailed(device->m_commandQueue->Signal(device->m_fence.Get(), device->m_fenceValues[device->m_frameIndex]));

		// Wait until the fence has been processed.
		ThrowIfFailed(device->m_fence->SetEventOnCompletion(device->m_fenceValues[device->m_frameIndex], device->m_fenceEvent));
		WaitForSingleObjectEx(device->m_fenceEvent, INFINITE, FALSE);

		// Increment the fence value for the current frame.
		device->m_fenceValues[device->m_frameIndex]++;
	}

	// Prepare to render the next frame.
	void MoveToNextFrame(display::Device* device)
	{
		// Schedule a Signal command in the queue.
		const UINT64 currentFenceValue = device->m_fenceValues[device->m_frameIndex];
		ThrowIfFailed(device->m_commandQueue->Signal(device->m_fence.Get(), currentFenceValue));

		// Update the frame index.
		device->m_frameIndex = device->m_swapChain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is ready.
		if (device->m_fence->GetCompletedValue() < device->m_fenceValues[device->m_frameIndex])
		{
			ThrowIfFailed(device->m_fence->SetEventOnCompletion(device->m_fenceValues[device->m_frameIndex], device->m_fenceEvent));
			WaitForSingleObjectEx(device->m_fenceEvent, INFINITE, FALSE);
		}

		// Set the fence value for the next frame.
		device->m_fenceValues[device->m_frameIndex] = currentFenceValue + 1;
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

		ThrowIfFailed(device->m_native_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&device->m_commandQueue)));

		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = static_cast<UINT>(params.num_frames);
		swapChainDesc.Width = static_cast<UINT>(params.width);
		swapChainDesc.Height = static_cast<UINT>(params.height);
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapChain;
		ThrowIfFailed(factory->CreateSwapChainForHwnd(
			device->m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
			platform::GetHwnd(),
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain
		));

		ThrowIfFailed(swapChain.As(&device->m_swapChain));
		device->m_frameIndex = device->m_swapChain->GetCurrentBackBufferIndex();

		// Create descriptor heaps.
		{
			// Describe and create a render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = static_cast<UINT>(params.num_frames);
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(device->m_native_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&device->m_rtvHeap)));

			device->m_rtvDescriptorSize = device->m_native_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// Create frame resources.
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(device->m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

			//Alloc rendertargets
			device->m_renderTargets.resize(params.num_frames);

			// Create a RTV for each frame.
			for (UINT n = 0; n < params.num_frames; n++)
			{
				ThrowIfFailed(device->m_swapChain->GetBuffer(n, IID_PPV_ARGS(&device->m_renderTargets[n])));
				device->m_native_device->CreateRenderTargetView(device->m_renderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, device->m_rtvDescriptorSize);
			}
		}

		ThrowIfFailed(device->m_native_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&device->m_commandAllocator)));

		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		{
			device->m_fenceValues.resize(params.num_frames);
			ThrowIfFailed(device->m_native_device->CreateFence(device->m_fenceValues[device->m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&device->m_fence)));
			device->m_fenceValues[device->m_frameIndex]++;

			// Create an event handle to use for frame synchronization.
			device->m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (device->m_fenceEvent == nullptr)
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

		CloseHandle(device->m_fenceEvent);

		delete device;
	}

	void Present(Device* device)
	{
		// Present the frame.
		ThrowIfFailed(device->m_swapChain->Present(1, 0));

		MoveToNextFrame(device);
	}
}