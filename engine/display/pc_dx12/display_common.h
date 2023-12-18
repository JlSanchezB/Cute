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
#include "D3D12MemAlloc.h"
#include <job/job_helper.h>
#include <core/fast_map.h>

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <string>
#include <wrl.h>
#include <shellapi.h>
#include <bitset>
#include <filesystem>
#include <dxcapi.h>
#include <unordered_set>


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
	template<typename HANDLE>
	class GraphicHandlePool : public core::HandlePool<HANDLE>
	{
	public:
		//Init
		void Init(size_t max_size, size_t init_size, size_t num_frames)
		{
			core::HandlePool<HANDLE>::Init(max_size, init_size);
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
				core::HandlePool<HANDLE>::Free(handle);
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
	template<typename HANDLE>
	class GraphicDescriptorHandlePool : public GraphicHandlePool<HANDLE>, public DescriptorHeapPool
	{
	public:
		void Init(size_t max_size, size_t init_size, size_t num_frames, Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type)
		{
			GraphicHandlePool<HANDLE>::Init(max_size, init_size, num_frames);
			AddHeap(device, heap_type, max_size);
		}

		void InitMultipleHeaps(size_t max_size, size_t init_size, size_t num_frames, Device* device, size_t num_heaps, D3D12_DESCRIPTOR_HEAP_TYPE* heap_type)
		{
			GraphicHandlePool<HANDLE>::Init(max_size, init_size, num_frames);
			for (size_t i = 0; i < num_heaps; ++i)
			{
				AddHeap(device, heap_type[i], max_size);
			}
		}

		void Destroy()
		{
			GraphicHandlePool<HANDLE>::Destroy();
			DestroyHeaps();
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptor(const Accessor& handle, size_t heap_index = 0)
		{
			return DescriptorHeapPool::GetDescriptor(GraphicHandlePool<HANDLE>::GetInternalIndex(handle), heap_index);
		}

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(const Accessor& handle, size_t heap_index = 0)
		{
			return DescriptorHeapPool::GetGPUDescriptor(GraphicHandlePool<HANDLE>::GetInternalIndex(handle), heap_index);
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
	template<typename HANDLE>
	class GraphicDescriptorHandleFreeList : public GraphicHandlePool<HANDLE>, public DescriptorHeapFreeList
	{
	public:
		void Init(size_t max_size, size_t init_size, size_t num_frames, size_t average_descriptors_per_handle, Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type)
		{
			GraphicHandlePool<HANDLE>::Init(max_size, init_size, num_frames);
			CreateHeap(device, heap_type, max_size * average_descriptors_per_handle);
		}

		void Destroy()
		{
			GraphicHandlePool<HANDLE>::Destroy();
			DestroyHeap();
		}

		//Allocate a handle with the descriptors associated to it
		template<typename ...Args>
		HANDLE Alloc(uint16_t num_descriptors, Args&&... args)
		{
			HANDLE handle = GraphicHandlePool<HANDLE>::Alloc(args...);
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

	//All resources will keep access to the memory of the resource
	struct ResourceMemoryAccess
	{
		void* memory_data = nullptr;
		size_t memory_size = 0;
		display::Access access;
	};

	//Ring resource support, used for dynamic type of resources
	template <typename HANDLE>
	struct RingResourceSupport : ResourceMemoryAccess
	{
		HANDLE next_handle;
	};

	//Internal context implementation
	struct DX12Context : Context
	{
		Device* device;
		ComPtr<ID3D12GraphicsCommandList> command_list;
		WeakRootSignatureHandle current_graphics_root_signature;
		WeakRootSignatureHandle current_compute_root_signature;
	};

	//Pool of resources
	struct CommandList
	{
		ComPtr<ID3D12GraphicsCommandList> resource;
		bool used_from_update;
	};
	struct RootSignature
	{
		ComPtr<ID3D12RootSignature> resource;
		RootSignatureDesc desc;
	};
	struct PipelineState : ComPtr<ID3D12PipelineState>
	{
	};
	struct PipelineReloadShaderData
	{
		std::string file_name;
		std::string entry_point;
		std::string target;
		std::string name;
		std::vector < std::pair<std::string, std::string>> defines;
		std::filesystem::file_time_type timestamp;
		std::vector<std::pair<std::string, std::filesystem::file_time_type>> include_timestaps;

		PipelineReloadShaderData()
		{
		}

		//Capture the compule shader desc for the reloading
		void Capture(const CompileShaderDesc& shader_descriptor, const std::unordered_set<std::string>& include_set)
		{
			if (shader_descriptor.file_name == nullptr)
			{
				//Nothing to as it is a shader blob, reloading is not going to work
				file_name = "";
				return;
			}
			file_name = shader_descriptor.file_name;
			entry_point = shader_descriptor.entry_point;
			target = shader_descriptor.target;
			if (shader_descriptor.name)
			{
				name = shader_descriptor.name;
			}
			for (size_t i = 0; i < shader_descriptor.defines.size(); ++i)
			{
				defines[i] = std::make_pair(shader_descriptor.defines[i].first, shader_descriptor.defines[i].second);
			}

			for (auto& include : include_set)
			{
				include_timestaps.emplace_back(include, std::filesystem::file_time_type());
			}

			UpdateTimeStamp();
		}

		void UpdateIncludeSet(const std::unordered_set<std::string>& include_set)
		{
			include_timestaps.clear();
			for (auto& include : include_set)
			{
				include_timestaps.emplace_back(include, std::filesystem::file_time_type());
			}
		}

		//Converts the captured data to a compile shader desc
		CompileShaderDesc GetCompileShaderDescriptor()
		{
			CompileShaderDesc ret;

			ret.file_name = file_name.c_str();
			ret.shader_code = nullptr;
			ret.entry_point = entry_point.c_str();
			ret.target = target.c_str();
			ret.name = name.c_str();
			for (size_t i = 0; i < defines.size(); ++i)
			{
				ret.defines[i] = std::make_pair(defines[i].first.c_str(), defines[i].second.c_str());
			}

			return ret;
		}

		bool NeedsUpdate() const
		{
			if (file_name == "") return false; //It was a blob shader
			if (timestamp < std::filesystem::last_write_time(file_name.c_str())) return true;
			for (auto& include : include_timestaps)
			{
				if (include.second < std::filesystem::last_write_time(include.first.c_str())) return true;
			}

			return false;
		}

		void UpdateTimeStamp()
		{
			timestamp = std::filesystem::last_write_time(file_name.c_str());

			for (auto& include : include_timestaps)
			{
				include.second = std::filesystem::last_write_time(include.first.c_str());
			}

		}
	};
	struct PipelineReloadData
	{
		WeakPipelineStateHandle handle;
		std::variant<D3D12_GRAPHICS_PIPELINE_STATE_DESC, D3D12_COMPUTE_PIPELINE_STATE_DESC> pipeline_desc;
		std::string name;
		std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements;
		std::vector<std::string> semantic_names;
		WeakRootSignatureHandle root_signature_handle;

		PipelineReloadShaderData VertexShaderCompileReloadData;
		PipelineReloadShaderData PixelShaderCompileReloadData;
		PipelineReloadShaderData ComputeShaderCompileReloadData;

		PipelineReloadData(const char* _name, WeakPipelineStateHandle _handle, const PipelineStateDesc& pipeline_state_desc, const D3D12_GRAPHICS_PIPELINE_STATE_DESC _pipeline_desc, std::vector<D3D12_INPUT_ELEMENT_DESC> _input_elements, const std::unordered_set<std::string>& vertex_shader_include_set, const std::unordered_set<std::string>& pixel_shader_include_set)
		{
			handle = _handle;
			name = _name;
			pipeline_desc = _pipeline_desc;
			input_elements = _input_elements;
			for (size_t i = 0; i < input_elements.size(); i++) semantic_names.push_back(input_elements[i].SemanticName);
			root_signature_handle = pipeline_state_desc.root_signature;
			VertexShaderCompileReloadData.Capture(pipeline_state_desc.vertex_shader, vertex_shader_include_set);
			PixelShaderCompileReloadData.Capture(pipeline_state_desc.pixel_shader, pixel_shader_include_set);
		}

		PipelineReloadData(const char* _name, WeakPipelineStateHandle _handle, const ComputePipelineStateDesc& pipeline_state_desc, const D3D12_COMPUTE_PIPELINE_STATE_DESC _pipeline_desc, const std::unordered_set<std::string>& include_set)
		{
			handle = _handle;
			name = _name;
			pipeline_desc = _pipeline_desc;
			root_signature_handle = pipeline_state_desc.root_signature;
			ComputeShaderCompileReloadData.Capture(pipeline_state_desc.compute_shader, include_set);
		}
	};

	struct Buffer : RingResourceSupport<BufferHandle>
	{
		static constexpr size_t kShaderResourceOrConstantBufferDescriptorIndex = 0;
		static constexpr size_t kShaderUnorderedAccessDescriptorIndex = 1;

		ComPtr<ID3D12Resource> resource;
		ComPtr<D3D12MA::Allocation> allocation;
		D3D12_RESOURCE_STATES current_state;

		BufferType type;

		bool UAV = false; //Can be set as UAV
		bool shader_access = false; //Can be set as shader resource or constant buffer
		const char* name;

		//Union of data for all the resource types
		union
		{
			D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
			D3D12_INDEX_BUFFER_VIEW index_buffer_view;
		};
	};

	struct Texture2D : RingResourceSupport<Texture2DHandle>
	{
		static constexpr size_t kShaderResourceDescriptorIndex = 0;
		static constexpr size_t kShaderUnorderedAccessDescriptorIndex = 1;
		static constexpr size_t kRenderTargetDescriptorIndex = 2;
		static constexpr size_t kDepthBufferDescriptorIndex = 3;

		ComPtr<ID3D12Resource> resource;
		ComPtr<D3D12MA::Allocation> allocation;
		D3D12_RESOURCE_STATES current_state;

		bool UAV = false; //Can be set as UAV
		bool RenderTarget = false; //Can be set as RenderTarget
		bool DepthBuffer = false; //Can be set as DepthBuffe
		const char* name;

		float default_depth;
		uint8_t default_stencil;
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

	//Device internal implementation
	struct Device
	{
		//DX12 device
		ComPtr<ID3D12Device> m_native_device;

		//Adapter desc
		DXGI_ADAPTER_DESC1 m_adapter_desc;
		char m_adapter_description[128];

		//Allocator
		D3D12MA::Allocator* m_allocator;

		//Device resources

		//Command allocator used for building command list during the update
		ComPtr<ID3D12CommandAllocator> m_main_thread_command_allocator;

		//Per frame system resources
		struct FrameResources
		{
			ComPtr<ID3D12CommandAllocator> command_allocator;
			UINT64 fence_value;
		};
		std::vector< FrameResources> m_frame_resources;
		bool m_before_first_frame = true;

		//Back buffer (ring resource buffer)
		Texture2DHandle m_back_buffer_render_target;

		//Global resources
		ComPtr<ID3D12CommandQueue> m_command_queue;
		ComPtr<IDXGISwapChain3> m_swap_chain;
		CommandListHandle m_present_command_list;
		CommandListHandle m_resource_command_list;

		// Synchronization objects.
		UINT m_frame_index;
		HANDLE m_fence_event;
		ComPtr<ID3D12Fence> m_fence;
		UINT64 m_fence_wait_offset = 0; //Offset between frame index and calls to wait for GPU
		bool m_tearing; //It changes all the fullscreen implementation
		bool m_windowed; //Only if tearing is not enabled
		bool m_vsync; //vsync
		size_t m_width;
		size_t m_height;
		bool m_debug_shaders;
		bool m_development_shaders;

		//Stadistics
		size_t uploaded_memory_frame = 0;

		//Shader compiler library
		ComPtr<IDxcUtils> m_shader_utils;
		ComPtr<IDxcCompiler3> m_shader_compiler;
		ComPtr<IDxcIncludeHandler> m_shader_default_include_handler;

		//Indirect draw command signatures
		ComPtr<ID3D12CommandSignature> m_indirect_draw_indexed_command_signature;
		ComPtr<ID3D12CommandSignature> m_indirect_draw_indexed_instanced_command_signature;
		ComPtr<ID3D12CommandSignature> m_indirect_execute_compute_command_signature;

		//Pool for context
		core::SimplePool<DX12Context, 256> m_context_pool;

		GraphicHandlePool<CommandListHandle> m_command_list_pool;
		GraphicHandlePool<RootSignatureHandle> m_root_signature_pool;
		GraphicHandlePool<PipelineStateHandle> m_pipeline_state_pool;
		GraphicDescriptorHandleFreeList<DescriptorTableHandle> m_descriptor_table_pool;
		GraphicDescriptorHandleFreeList<SamplerDescriptorTableHandle> m_sampler_descriptor_table_pool;

		GraphicDescriptorHandlePool<BufferHandle> m_buffer_pool;
		GraphicDescriptorHandlePool<Texture2DHandle> m_texture_2d_pool;

		//Reload data for pipeline states
		std::vector<PipelineReloadData> m_pipeline_reload_data;

		//Development shaders
		BufferHandle m_development_shaders_buffer;
		size_t m_development_shaders_buffer_capacity = 0;
		BufferHandle m_development_shaders_readback_buffer;

		//Map of the indexes of all the GPU control variables
		struct ControlVariable
		{
			size_t index;
			std::variant<float, uint32_t, bool> default_value;
		};
		core::FastMap<std::string, ControlVariable> m_control_variables;

		//Map of the indexes for all GPU stats
		core::FastMap<std::string, size_t> m_stats;

		//Accesor to the resources (we need a specialitation for each type)
		template<typename HANDLE>
		auto& Get(const HANDLE&);

		//Deferred delete resource ring buffer
		struct DeferredResourceDelete
		{
			//Resource to delete
			ComPtr<ID3D12Object> resource;
			//Allocation
			ComPtr<D3D12MA::Allocation> allocation;
			//Value signal to the fence
			UINT64 fence_value;

			DeferredResourceDelete()
			{
			}

			DeferredResourceDelete(ComPtr<ID3D12Object>& _resource, ComPtr<D3D12MA::Allocation>& _allocation, UINT64 _fence_value) : resource(_resource), allocation(_allocation), fence_value(_fence_value)
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

		size_t m_upload_buffer_max_size;
		//Small Upload buffer
		struct PooledUploadBuffer
		{
			//Allocation, the buffer is inside
			ComPtr<D3D12MA::Allocation> allocation;
			//Used frame, it will be free when the GPU is over this, if it is the frame index
			uint64_t frame;
			//Memory access
			ResourceMemoryAccess memory_access;
		};
		std::vector<PooledUploadBuffer> m_upload_buffer_pool;
		core::Mutex m_update_buffer_pool_mutex;

		//Current active Upload buffers
		struct ActiveUploadBuffer
		{
			//Allocation, the buffer is inside
			ComPtr<D3D12MA::Allocation> allocation;
			//Current offset
			size_t current_offset = 0;
			//Memory access
			ResourceMemoryAccess memory_access;
			//Index in the pool 
			size_t pool_index = -1;
		};
		job::ThreadData<ActiveUploadBuffer> m_active_upload_buffers;

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

	template<>
	inline auto& Device::Get<BufferHandle>(const BufferHandle& handle)
	{
		return this->m_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakBufferHandle>(const WeakBufferHandle& handle)
	{
		return this->m_buffer_pool[handle];
	}

	template<>
	inline auto& Device::Get<Texture2DHandle>(const Texture2DHandle& handle)
	{
		return this->m_texture_2d_pool[handle];
	}

	template<>
	inline auto& Device::Get<WeakTexture2DHandle>(const WeakTexture2DHandle& handle)
	{
		return this->m_texture_2d_pool[handle];
	}

	//Delete resources that are not needed by the GPU
	size_t DeletePendingResources(display::Device* device);

	void AddDeferredDeleteResource(display::Device* device, ComPtr<ID3D12Object>& resource, ComPtr<D3D12MA::Allocation>& allocation);

	//Add a resource to be deleted, only to be called if you are sure that you don't need the resource
	template <typename RESOURCE>
	void AddDeferredDeleteResource(display::Device* device, RESOURCE&& resource, ComPtr<D3D12MA::Allocation>& allocation)
	{
		ComPtr<ID3D12Object> object;
		ThrowIfFailed(resource.As(&object));

		AddDeferredDeleteResource(device, object, allocation);
	}

	template <typename RESOURCE>
	void AddDeferredDeleteResource(display::Device* device, RESOURCE&& resource)
	{
		ComPtr<ID3D12Object> object;
		ThrowIfFailed(resource.As(&object));

		ComPtr<D3D12MA::Allocation> null_allocation;

		AddDeferredDeleteResource(device, object, null_allocation);
	}

	//Allocate memory in the upload heap, returns the resource and the offset inside the resource and the mapped memory
	//That memory can only be written and it will be valid just for the current frame
	struct AllocationUploadBuffer
	{
		size_t offset;
		ComPtr<ID3D12Resource> resource;
		void* memory;
	};
	AllocationUploadBuffer AllocateUploadBuffer(display::Device* device, size_t size);

	//Upload pool buffers begin frame
	void UploadBufferReset(display::Device* device);

	//Destroy upload buffer pool
	void DestroyUploadBufferPool(display::Device* device);

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

	template<typename COM>
	inline void SafeRelease(COM& com_ptr)
	{
		com_ptr.Reset();
	}
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;