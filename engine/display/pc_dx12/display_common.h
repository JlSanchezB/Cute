#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif

#include <display\display.h>
#include "d3dx12.h"
#include "display_convert.h"
#include <core/ring_buffer.h>
#include <core/simple_pool.h>

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <wrl.h>
#include <shellapi.h>
#include <bitset>

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
	//Graphics handle pool, it is a handle pool with deferred deallocation
	template<typename HANDLE, typename DATA>
	class GraphicHandlePool : public core::HandlePool<HANDLE, DATA>
	{
	public:
		//Init
		void Init(size_t max_size, size_t init_size, size_t num_frames)
		{
			core::HandlePool<HANDLE, DATA>::Init(max_size, init_size);
			m_deferred_delete_handles.resize(num_frames);
		}

		//Free unused handle
		//It will get added into a ring buffer
		void Free(HANDLE& handle)
		{
			if (handle.IsValid())
			{
				//Add to the current frame
				m_deferred_delete_handles[m_current_frame].push_back(std::move(handle));

				//It will invalidate the handle, from outside will look like deleted
			}
		}

		//Call each frame for cleaning
		void NextFrame()
		{
			//Deallocate all handles
			size_t last_frame = (m_current_frame + 1) % m_deferred_delete_handles.size();
			//Destroy all handles
			for (auto& handle : m_deferred_delete_handles[last_frame])
			{
				DeferredFree(handle);
				core::HandlePool<HANDLE, DATA>::Free(handle);
			}
			m_deferred_delete_handles[last_frame].clear();
			//New frame is the last frame
			m_current_frame = last_frame;
		}

		//Clear all handles
		void Destroy()
		{
			size_t num_frames = m_deferred_delete_handles.size();
			for (size_t i = 0; i < num_frames; ++i)
			{
				NextFrame();
			}
		}
	protected:
		virtual void DeferredFree(HANDLE& handle)
		{
		};
	private:

		size_t m_current_frame = 0;
		std::vector<std::vector<HANDLE>> m_deferred_delete_handles;
	};

	//GraphicsHandlePool with descriptor heap pool support
	template<typename HANDLE, typename DATA>
	class GraphicDescriptorHandlePool : public GraphicHandlePool<HANDLE, DATA>, public DescriptorHeapPool
	{
	public:
		void Init(size_t max_size, size_t init_size, size_t num_frames, Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type)
		{
			GraphicHandlePool<HANDLE, DATA>::Init(max_size, init_size, num_frames);
			AddHeap(device, heap_type, max_size);
		}

		void InitMultipleHeaps(size_t max_size, size_t init_size, size_t num_frames, Device* device, size_t num_heaps, D3D12_DESCRIPTOR_HEAP_TYPE* heap_type)
		{
			GraphicHandlePool<HANDLE, DATA>::Init(max_size, init_size, num_frames);
			for (size_t i = 0; i < num_heaps; ++i)
			{
				AddHeap(device, heap_type[i], max_size);
			}
		}

		~GraphicDescriptorHandlePool()
		{
			DestroyHeaps();
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(const Accessor& handle, size_t heap_index = 0)
		{
			return DescriptorHeapPool::GetDescriptor(GraphicHandlePool<HANDLE, DATA>::GetInternalIndex(handle), heap_index);
		}

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(const Accessor& handle, size_t heap_index = 0)
		{
			return DescriptorHeapPool::GetGPUDescriptor(GraphicHandlePool<HANDLE, DATA>::GetInternalIndex(handle), heap_index);
		}
	};

	template<typename HANDLE>
	struct GraphicHandlePoolEmptyFreeFunction
	{
		void Free(HANDLE& handle)
		{

		}
	};

	//GraphicsHandlePool with descriptor heap free list support
	template<typename HANDLE, typename DATA>
	class GraphicDescriptorHandleFreeList : public GraphicHandlePool<HANDLE, DATA>, public DescriptorHeapFreeList
	{
	public:
		void Init(size_t max_size, size_t init_size, size_t num_frames, size_t average_descriptors_per_handle, Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type)
		{
			GraphicHandlePool<HANDLE, DATA>::Init(max_size, init_size, num_frames);
			CreateHeap(device, heap_type, max_size * average_descriptors_per_handle);
		}

		~GraphicDescriptorHandleFreeList()
		{
			DestroyHeap();
		}

		//Allocate a handle with the descriptors associated to it
		template<typename ...Args>
		HANDLE Alloc(uint16_t num_descriptors, Args&&... args)
		{
			HANDLE handle = GraphicHandlePool<HANDLE, DATA>::Alloc(args...);
			DescriptorHeapFreeList::AllocDescriptors(operator[](handle), num_descriptors);
			return handle;
		}

		//Free unused handle
		void DeferredFree(HANDLE& handle) override
		{
			DescriptorHeapFreeList::DeallocDescriptors(operator[](handle));
		};

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(const Accessor& handle, size_t offset = 0)
		{
			return DescriptorHeapFreeList::GetDescriptor(operator[](handle), offset);
		}

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(const Accessor& handle, size_t offset = 0)
		{
			return DescriptorHeapFreeList::GetGPUDescriptor(operator[](handle), offset);
		}
	};

	//Ring resource support, used for dynamic type of resources
	template <typename HANDLE>
	struct RingResourceSupport
	{
		HANDLE next_handle;
		void* memory_data = nullptr;
		size_t memory_size = 0;
	};

	//Internal context implementation
	struct DX12Context : Context
	{
		Device* device;
		ComPtr<ID3D12GraphicsCommandList> command_list;
		WeakRootSignatureHandle current_graphics_root_signature;
		WeakRootSignatureHandle current_compute_root_signature;
	};

	//Device internal implementation
	struct Device
	{
		//DX12 device
		ComPtr<ID3D12Device> m_native_device;

		//Adapter desc
		DXGI_ADAPTER_DESC1 m_adapter_desc;
		char m_adapter_description[128];

		//Device resources

		//Per frame system resources
		struct FrameResources
		{
			ComPtr<ID3D12CommandAllocator> command_allocator;
			UINT64 fence_value;
		};
		std::vector< FrameResources> m_frame_resources;

		//Back buffer (ring resource buffer)
		RenderTargetHandle m_back_buffer_render_target;

		//Global resources
		ComPtr<ID3D12CommandQueue> m_command_queue;
		ComPtr<IDXGISwapChain3> m_swap_chain;
		CommandListHandle m_present_command_list;
		CommandListHandle m_resource_command_list;

		// Synchronization objects.
		UINT m_frame_index;
		HANDLE m_fence_event;
		ComPtr<ID3D12Fence> m_fence;
		bool m_tearing; //It changes all the fullscreen implementation
		bool m_windowed; //Only if tearing is not enabled
		bool m_vsync; //vsync
		size_t m_width;
		size_t m_height;

		//Statis
		size_t uploaded_memory_frame = 0;


		//Pool for context
		core::SimplePool<DX12Context, 256> m_context_pool;

		//Pool of resources
		struct CommandList
		{
			ComPtr<ID3D12GraphicsCommandList> resource;
		};
		struct RootSignature
		{
			ComPtr<ID3D12RootSignature> resource;
			RootSignatureDesc desc;
		};
		using PipelineState = ComPtr<ID3D12PipelineState>;
		struct RenderTarget : RingResourceSupport<RenderTargetHandle>
		{
			ComPtr<ID3D12Resource> resource;
			D3D12_RESOURCE_STATES current_state;
		};
		struct DepthBuffer : RingResourceSupport<DepthBufferHandle>
		{
			ComPtr<ID3D12Resource> resource;
			D3D12_RESOURCE_STATES current_state;
		};
		struct VertexBuffer : RingResourceSupport<VertexBufferHandle>
		{
			ComPtr<ID3D12Resource> resource;
			D3D12_VERTEX_BUFFER_VIEW view;
		};
		struct IndexBuffer : RingResourceSupport<IndexBufferHandle>
		{
			ComPtr<ID3D12Resource> resource;
			D3D12_INDEX_BUFFER_VIEW view;
		};
		struct ConstantBuffer : RingResourceSupport<ConstantBufferHandle>
		{
			ComPtr<ID3D12Resource> resource;
		};
		struct UnorderedAccessBuffer
		{
			ComPtr<ID3D12Resource> resource;
		};
		struct ShaderResource : RingResourceSupport<ShaderResourceHandle>
		{
			ComPtr<ID3D12Resource> resource;
		};
		struct DescriptorTable : DescriptorHeapFreeList::Block, RingResourceSupport<DescriptorTableHandle>
		{
		};
		struct Sampler
		{
		};
		struct SamplerDescriptorTable : DescriptorHeapFreeList::Block
		{
		};
		

		GraphicHandlePool<CommandListHandle, CommandList> m_command_list_pool;
		GraphicDescriptorHandlePool<RenderTargetHandle, RenderTarget> m_render_target_pool;
		GraphicDescriptorHandlePool<DepthBufferHandle, DepthBuffer> m_depth_buffer_pool;
		GraphicHandlePool<RootSignatureHandle, RootSignature> m_root_signature_pool;
		GraphicHandlePool<PipelineStateHandle, PipelineState> m_pipeline_state_pool;
		GraphicHandlePool<VertexBufferHandle, VertexBuffer> m_vertex_buffer_pool;
		GraphicHandlePool<IndexBufferHandle, IndexBuffer> m_index_buffer_pool;
		GraphicDescriptorHandlePool<ConstantBufferHandle, ConstantBuffer> m_constant_buffer_pool;
		GraphicDescriptorHandlePool<UnorderedAccessBufferHandle, UnorderedAccessBuffer> m_unordered_access_buffer_pool;
		GraphicDescriptorHandlePool<ShaderResourceHandle, ShaderResource> m_shader_resource_pool;
		GraphicDescriptorHandleFreeList<DescriptorTableHandle, DescriptorTable> m_descriptor_table_pool;
		GraphicDescriptorHandleFreeList<SamplerDescriptorTableHandle, SamplerDescriptorTable> m_sampler_descriptor_table_pool;

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

		//Last error
		static constexpr size_t kLastErrorBufferSize = 1024;
		char m_last_error_message[kLastErrorBufferSize] = "";
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
	inline auto& Device::Get<DepthBufferHandle>(const DepthBufferHandle& handle)
	{
		return this->m_depth_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakDepthBufferHandle>(const WeakDepthBufferHandle& handle)
	{
		return this->m_depth_buffer_pool[handle];
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
	inline auto& Device::Get<ShaderResourceHandle>(const ShaderResourceHandle& handle)
	{
		return this->m_shader_resource_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakShaderResourceHandle>(const WeakShaderResourceHandle& handle)
	{
		return this->m_shader_resource_pool[handle];
	}

	template<>
	inline auto& Device::Get<DescriptorTableHandle>(const DescriptorTableHandle& handle)
	{
		return this->m_descriptor_table_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakDescriptorTableHandle>(const WeakDescriptorTableHandle& handle)
	{
		return this->m_descriptor_table_pool[handle];
	}

	template<>
	inline auto& Device::Get<SamplerDescriptorTableHandle>(const SamplerDescriptorTableHandle& handle)
	{
		return this->m_sampler_descriptor_table_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakSamplerDescriptorTableHandle>(const WeakSamplerDescriptorTableHandle& handle)
	{
		return this->m_sampler_descriptor_table_pool[handle];
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

	//Return the handle used in the current frame of a ring resource
	template<typename WEAKHANDLE>
	WEAKHANDLE GetRingResource(display::Device * device, WEAKHANDLE handle, size_t frame_index)
	{
		//Only if it is a ring resource
		if (device->Get(handle).next_handle.IsValid())
		{
			//The frame index indicate how many jumps are needed
			size_t count = frame_index;

			while (count)
			{
				handle = device->Get(handle).next_handle;
				count--;
			}
		}

		return handle;
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

	inline void SetObjectName(ID3D12Object* object, const char* name)
	{
		if (name)
		{
			wchar_t wstr[4096];
			size_t out_size;
			mbstowcs_s(&out_size, wstr, 4096, name, strlen(name));
			object->SetName(wstr);
		}
	}

	inline void SetLastErrorMessage(display::Device* device, const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		int w = vsnprintf_s(device->m_last_error_message, display::Device::kLastErrorBufferSize, display::Device::kLastErrorBufferSize - 1, message, args);

		if (w == -1 || w >= static_cast<int>(display::Device::kLastErrorBufferSize))
			w = static_cast<int>(display::Device::kLastErrorBufferSize - 1);
		device->m_last_error_message[w] = 0;
		va_end(args);

		core::LogError("Error reported from display <%s>", device->m_last_error_message);
	}
}

#define SAFE_RELEASE(p) if (p) (p)->Release()

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;