//////////////////////////////////////////////////////////////////////////
// Cute engine - Manager of the render system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_H_
#define RENDER_H_

#include <render/render_common.h>
#include <render/render_frame.h>
#include <optional>

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
	class ContextPass;

	struct SystemDesc
	{
	};

	//Renderer allows to add modules that can register new passes and resources
	class Module
	{
	public:
		virtual ~Module() {};
		virtual void Init(display::Device* device, System* system) {};
		virtual void Shutdown(display::Device* device, System* system) {};
		virtual void BeginFrame(display::Device* device, System* system, uint64_t cpu_frame_index, uint64_t freed_frame_index) {};
		virtual void EndFrame(display::Device* device, System* system) {};
	};

	//Context used for rendering a pass
	//It will include all the information that the render pass manager needs for rendering a pass
	class RenderContext
	{
	public:
		//Add resource
		bool AddPassResource(const ResourceName& name, std::unique_ptr<Resource>&& resource);

		//Resource
		Resource* GetResource(const ResourceName& name, bool& can_not_be_cached) const;

		template<typename RESOURCE>
		inline RESOURCE* GetResource(const ResourceName& name, bool& can_not_be_cached) const
		{
			Resource* resource = GetResource(name, can_not_be_cached);
			if (resource && resource->Type() == RESOURCE::kClassName)
			{
				return dynamic_cast<RESOURCE*>(resource);
			}
			return nullptr;
		}

		//Get Render frame
		Frame& GetRenderFrame();

		//Get root pass been rendering
		ContextPass* GetContextRootPass() const;

		//Get display device
		display::Device* GetDevice() const;

		//Get render system
		render::System* GetRenderSystem() const;

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
	bool AddGameResource(System* system, const ResourceName& name, std::unique_ptr<Resource>&& resource, const std::optional<display::TranstitionState>& current_access = {});

	//Add game resource associated to a pass and pass id, allows the game to add global resources that the pass system can access them
	bool AddGameResource(System* system, const ResourceName& name, const PassName& pass_name, const uint16_t pass_id, std::unique_ptr<Resource>&& resource, const std::optional<display::TranstitionState>& current_access = {});

	//Update access in a game resorce, transition has to be done outside of the render
	void UpdateGameResourceAccess(System* system, const ResourceName& name, const display::TranstitionState& access);


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
	Priority GetRenderItemPriority(System* system, const PriorityName priority_name);

	//Get module
	Module* GetModule(System* system, const ModuleName name);

	template<typename MODULE>
	MODULE* GetModule(System* system, const ModuleName name)
	{
		return dynamic_cast<MODULE*>(GetModule(system, name));
	}

	//Register render module
	void RegisterModule(System* system, const ModuleName name, std::unique_ptr<Module>&& module);

	template<typename MODULE, typename ...ARGS>
	MODULE* RegisterModule(System* system, const ModuleName name, ARGS&& ...args)
	{
		RegisterModule(system, name, std::make_unique<MODULE>(std::forward<ARGS>(args)...));

		return dynamic_cast<MODULE*>(GetModule(system, name));
	}
}

#endif //RENDER_H_
