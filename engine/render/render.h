//////////////////////////////////////////////////////////////////////////
// Cute engine - Manager of the render system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_H_
#define RENDER_H_

#include <render/render_common.h>
#include <render/render_frame.h>

namespace job
{
	struct System;
}

namespace platform
{
	class Game;
}

namespace render
{
	using AllocHandle = core::Handle<struct FreeListAllocation, uint16_t>;
	using WeakAllocHandle = core::WeakHandle<struct FreeListAllocation, uint16_t>;

	struct SystemDesc
	{
		uint32_t static_gpu_memory_size = 128 * 1024;
		uint32_t dynamic_gpu_memory_size = 128 * 1024;
		uint32_t dynamic_gpu_memory_segment_size =  4 * 1024;
	};

	//Context used for rendering a pass
	//It will include all the information that the render pass manager needs for rendering a pass
	class RenderContext
	{
	public:
		//Resource
		Resource* GetResource(const ResourceName& name) const;

		template<typename RESOURCE>
		inline RESOURCE* GetResource(const ResourceName& name) const
		{
			Resource* resource = GetResource(name);
			if (resource && resource->Type() == RESOURCE::kClassName)
			{
				return dynamic_cast<RESOURCE*>(resource);
			}
			return nullptr;
		}

		//Get Render frame
		Frame& GetRenderFrame();

		//Get root pass been rendering
		Pass* GetRootPass() const;

		//Get display device
		display::Device* GetDevice() const;

		//Get display Context
		display::Context* GetContext() const;

		//Get pass info
		const PassInfo& GetPassInfo() const;

		//Set display context
		void SetContext(display::Context* context);

		//Update pass info
		void UpdatePassInfo(const PassInfo& pass_info);
	};

	//Create render system
	System* CreateRenderSystem(display::Device* device, job::System* job_system = nullptr, platform::Game* game = nullptr, const SystemDesc& desc = SystemDesc());

	//Destroy render system
	void DestroyRenderSystem(System*& system, display::Device* device);

	//Load render pass descriptor file
	bool LoadPassDescriptorFile(System* system, display::Device* device, const char* descriptor_file_buffer, size_t descriptor_file_buffer_size, std::vector<std::string>& errors);

	//Add game resource, allows the game to add global resources that the pass system can access them
	bool AddResource(System* system, const ResourceName& name, std::unique_ptr<Resource>&& resource);

	//Register resource factory
	bool RegisterResourceFactory(System* system, const RenderClassType& resource_type, std::unique_ptr<FactoryInterface<Resource>>& resource_factory);

	//Register pass factory
	bool RegisterPassFactory(System* system, const RenderClassType& pass_type, std::unique_ptr<FactoryInterface<Pass>>& pass_factory);

	//Register resource factory helper
	template<typename RESOURCE>
	inline bool RegisterResourceFactory(System* system)
	{
		std::unique_ptr<FactoryInterface<Resource>> factory = std::make_unique<Factory<Resource, RESOURCE>>();
		return RegisterResourceFactory(system, RESOURCE::kClassName, factory);
	}

	//Register pass factory helper
	template<typename PASS>
	inline bool RegisterPassFactory(System* system)
	{
		std::unique_ptr<FactoryInterface<Pass>> factory = std::make_unique<Factory<Pass, PASS>>();
		return RegisterPassFactory(system, PASS::kClassName, factory);
	}

	//Get Resource by name
	Resource* GetResource(System* system, const ResourceName& name);

	template<typename RESOURCE>
	inline RESOURCE* GetResource(System* system, const ResourceName& name)
	{
		Resource* resource = GetResource(system, name);
		if (resource && resource->Type() == RESOURCE::kClassName)
		{
			return dynamic_cast<RESOURCE*>(resource);
		}
		return nullptr;
	}

	//Get Pass by name
	Pass* GetPass(System* system, const PassName& name);

	//Create a render context for rendering a pass
	RenderContext* CreateRenderContext(System* system, display::Device* device, const PassName& pass, const PassInfo& pass_info, ResourceMap& init_resources, std::vector<std::string>& errors);

	//Destroy render context for rendering a pass
	void DestroyRenderContext(System* system, RenderContext*& render_context);

	//Capture render context
	void CaptureRenderContext(System* system, RenderContext* render_context);

	//Execute render context
	void ExecuteRenderContext(System* system, RenderContext* render_context);

	//Begin prepare render, the game thread can start to summit all the point of views and render items
	//Thread can be locked if there is not sufficient frame buffers
	void BeginPrepareRender(System* system);

	//End prepare, so game thread can jump to the next frame
	//The render is going to be submit to the GPU (on other thread)
	void EndPrepareRenderAndSubmit(System* system);

	//Return the current frame for the game
	uint64_t GetGameFrameIndex(System* system);

	//Return the current frame for the render 
	uint64_t GetRenderFrameIndex(System* system);

	//Get render frame for this frame
	//Only can be called from the game thread, between begin and end prepare frame
	Frame& GetGameRenderFrame(System* system);

	//Get the index of the priority for a priority name
	Priority GetRenderItemPriority(System* system, PriorityName priority_name);

	//Allocate a buffer in the static gpu memory
	AllocHandle AllocStaticGPUMemory(System* system, const size_t size, const void* data, const uint64_t frame_index);

	//Deallocate static gpu memory
	void DeallocStaticGPUMemory(System* system, AllocHandle&& handle, const uint64_t frame_index);

	//Update Static GPU memory
	void UpdateStaticGPUMemory(System* system, const AllocHandle& handle, const void* data, const size_t size, const uint64_t frame_index);

	//Alloc dynamic gpu memory
	void* AllocDynamicGPUMemory(System* system, const size_t size, const uint64_t frame_index);

	//Get static gpu memory resource
	display::WeakUnorderedAccessBufferHandle GetStaticGPUMemoryResource(System* system);

	//Get dynamic gpu memory resource
	display::WeakShaderResourceHandle GetDynamicGPUMemoryResource(System* system);
}

#endif //RENDER_H_
