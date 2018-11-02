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

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
using Microsoft::WRL::ComPtr;

//Added internal classes used by display
#include "descriptor_heap.h"

namespace display
{
	//State of the properties to be send to the GPU
	struct GraphicsState
	{
		static const size_t kNumMaxProperties = 32;

		std::array<WeakConstantBufferHandle, kNumMaxProperties> constant_buffers = {};
		std::array<WeakUnorderedAccessBufferHandle, kNumMaxProperties> unordered_access_buffers = {};
		std::array<WeakTextureHandle, kNumMaxProperties> textures = {};
	};

	//State of the properties that has been set in the root signature
	struct RootSignatureState
	{
		struct Property
		{
			D3D12_GPU_VIRTUAL_ADDRESS address = 0;
		};
		std::vector<Property> properties;
	};

	//Graphics handle pool, it is a handle pool with deferred deallocation
	template<typename HANDLE, typename DATA>
	class GraphicHandlePool : public core::HandlePool<HANDLE, DATA>
	{
	public:
		//Init
		void Init(size_t max_size, size_t init_size, size_t num_frames)
		{
			core::HandlePool<HANDLE, DATA>::Init(max_size, init_size);
			m_defered_delete_handles.resize(num_frames);
		}

		//Free unused handle
		//It will get added into a ring buffer
		void Free(HANDLE& handle)
		{
			//Add to the current frame
			m_defered_delete_handles[m_current_frame].push_back(std::move(handle));

			//It will invalidate the handle, from outside will look like deleted
		}

		//Call each frame for cleaning
		void NextFrame()
		{
			//Deallocate all handles
			size_t last_frame = (m_current_frame + 1) % m_defered_delete_handles.size();
			//Destroy all handles
			for (auto& handle : m_defered_delete_handles[last_frame])
			{
				core::HandlePool<HANDLE, DATA>::Free(handle);
			}
			m_defered_delete_handles[last_frame].clear();
			//New frame is the last frame
			m_current_frame = last_frame;
		}

		//Clear all handles
		void Destroy()
		{
			size_t num_frames = m_defered_delete_handles.size();
			for (size_t i = 0; i < num_frames; ++i)
			{
				NextFrame();
			}
		}
	private:

		size_t m_current_frame = 0;
		std::vector<std::vector<HANDLE>> m_defered_delete_handles;
	};

	//GraphicsHandlePool with descriptor heap support
	template<typename HANDLE, typename DATA>
	class GraphicDescriptorHandlePool : public GraphicHandlePool<HANDLE, DATA>, public DescriptorHeapPool
	{
	public:
		void Init(size_t max_size, size_t init_size, size_t num_frames, Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type)
		{
			GraphicHandlePool<HANDLE, DATA>::Init(max_size, init_size, num_frames);
			CreateHeap(device, heap_type, max_size);
		}

		~GraphicDescriptorHandlePool()
		{
			DestroyHeap();
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(const HANDLE& handle)
		{
			return DescriptorHeapPool::GetDescriptor(GraphicHandlePool<HANDLE, DATA>::GetInternalIndex(handle));
		}
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
			RootSignatureState root_signature_state;
			GraphicsState graphics_state;
			RootSignatureDesc root_signature_desc;
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

		GraphicHandlePool<CommandListHandle, CommandList> m_command_list_pool;
		GraphicDescriptorHandlePool<RenderTargetHandle, RenderTarget> m_render_target_pool;
		GraphicHandlePool<RootSignatureHandle, RootSignature> m_root_signature_pool;
		GraphicHandlePool<PipelineStateHandle, PipelineState> m_pipeline_state_pool;
		GraphicHandlePool<VertexBufferHandle, VertexBuffer> m_vertex_buffer_pool;
		GraphicHandlePool<IndexBufferHandle, IndexBuffer> m_index_buffer_pool;
		GraphicHandlePool<ConstantBufferHandle, ConstantBuffer> m_constant_buffer_pool;
		GraphicHandlePool<UnorderedAccessBufferHandle, UnorderedAccessBuffer> m_unordered_access_buffer_pool;
		GraphicHandlePool<TextureHandle, TextureBuffer> m_texture_pool;

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

	//Delete resources that are not needed by the GPU
	size_t DeletePendingResources(display::Device* device);

	void AddDeferredDeleteResource(display::Device* device, ComPtr<ID3D12Object> resource);

	//Add a resource to be deleted, only to be called if you are sure that you don't need the resource
	template <typename RESOURCE>
	void AddDeferredDeleteResource(display::Device* device, RESOURCE&& resource)
	{
		ComPtr<ID3D12Object> object;
		ThrowIfFailed(resource.As(&object));

		AddDeferredDeleteResource(device, object);
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

	inline ComPtr<ID3D12CommandAllocator>& GetCommandAllocator(display::Device* device)
	{
		return device->m_frame_resources[device->m_frame_index].command_allocator;
	}
}

#define SAFE_RELEASE(p) if (p) (p)->Release()