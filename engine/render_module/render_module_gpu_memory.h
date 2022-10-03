//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for the GPU memory model used for sending data from CPU to GPU
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_MODULE_GPU_MEMORY_H_
#define RENDER_MODULE_GPU_MEMORY_H_

#include <render/render.h>
#include <display/display_handle.h>
#include <job/job_helper.h>
#include <render_module/render_freelist_allocator.h>
#include <render_module/render_segment_allocator.h>


namespace display
{
	struct Device;
	struct Context;
}

namespace render
{
	//Render GPU memory offers the user a gpu memory for uploading data from the cpu

	//Static data is GPU only memory, only need to be sended once and the user can allocate a piece of it
	//Static data must keep static between frames, but still can be modified.

	//Dynamic data only exist during the frame that is allocated
	//Users doesn't need to keep track of this memory, will be avaliable until the gpu uses it
	class GPUMemoryRenderModule : public render::Module
	{
		struct CopyDataCommand
		{
			uint32_t source_offset;		//In dynamic gpu memory
			uint32_t dest_offset;		//In static gpu memory
			uint32_t size;

			CopyDataCommand(const uint32_t _source_offset, const uint32_t _dest_offset, const uint32_t _size) :
				source_offset(_source_offset), dest_offset(_dest_offset), size(_size)
			{
			}
		};

		//Static buffer resource in the GPU
		display::UnorderedAccessBufferHandle m_static_gpu_memory_buffer;
		//Static gpu allocator
		FreeListAllocator m_static_gpu_memory_allocator;

		//Dynamic buffer resource in the GPU
		display::ShaderResourceHandle m_dynamic_gpu_memory_buffer;
		//Dynamic buffer resource in the GPU
		SegmentAllocator m_dynamic_gpu_memory_allocator;

		//Copy compute shader
		display::RootSignatureHandle m_copy_data_compute_root_signature;
		display::PipelineStateHandle m_copy_data_compute_pipeline_state;


		//An over approximation of max distance between CPU and GPU
		//That is from the GAME thread to the GPU
		static constexpr size_t kMaxFrames = 8;

		//Copy data commands per frame and worker
		std::array<job::ThreadData<std::vector<CopyDataCommand>>, kMaxFrames> m_copy_data_commands;

		template<typename ...Args>
		void AddCopyDataCommand(const uint64_t frame_index, Args&& ...args)
		{
			m_copy_data_commands[frame_index % GPUMemoryRenderModule::kMaxFrames].Get().emplace_back(std::forward<Args>(args)...);
		}

		uint8_t* m_dynamic_gpu_memory_base_ptr = nullptr;

		uint32_t m_static_gpu_memory_size;
		uint32_t m_dynamic_gpu_memory_size;
		uint32_t m_dynamic_gpu_memory_segment_size;

		//Capture all copies for this frame and execute the command list
		void ExecuteGPUCopy(uint64_t frame_index, display::Context* display_context);

		friend class SyncStaticGPUMemoryPass;

	public:
		struct GPUMemoryDesc
		{
			uint32_t static_gpu_memory_size = 128 * 1024;
			uint32_t dynamic_gpu_memory_size = 128 * 1024;
			uint32_t dynamic_gpu_memory_segment_size = 4 * 1024;
		};

		GPUMemoryRenderModule(const GPUMemoryDesc& desc);
		
		void Init(display::Device* device, System* system) override;
		void Shutdown(display::Device* device, System* system) override;
		void BeginFrame(display::Device* device, System* system, uint64_t cpu_frame_index, uint64_t freed_frame_index) override;

		//Allocate a buffer in the static gpu memory
		AllocHandle AllocStaticGPUMemory(display::Device* device, const size_t size, const void* data, const uint64_t frame_index);

		//Deallocate static gpu memory
		void DeallocStaticGPUMemory(display::Device* device, AllocHandle& handle, const uint64_t frame_index);

		//Update Static GPU memory
		void UpdateStaticGPUMemory(display::Device* device, const AllocHandle& handle, const void* data, const size_t size, const uint64_t frame_index, size_t destination_offset = 0);

		//Alloc dynamic gpu memory
		void* AllocDynamicGPUMemory(display::Device* device, const size_t size, const uint64_t frame_index);

		//Get static gpu memory resource
		display::WeakUnorderedAccessBufferHandle GetStaticGPUMemoryResource();

		//Get offset from a AllocHandle
		size_t GetStaticGPUMemoryOffset(const AllocHandle& handle) const;

		//Get offset from a allocation in the Dynamic GPU memory
		size_t GetDynamicGPUMemoryOffset(display::Device* device, void* allocation) const;

		//Get dynamic gpu memory resource
		display::WeakShaderResourceHandle GetDynamicGPUMemoryResource();
	};

	class SyncStaticGPUMemoryPass : public Pass
	{
		mutable GPUMemoryRenderModule* m_gpu_memory_render_module = nullptr;
	public:
		DECLARE_RENDER_CLASS("SyncStaticGPUMemoryPass");
	
		void Render(RenderContext& render_context) const override;
	};
}

#endif //RENDER_MODULE_GPU_MEMORY_H_