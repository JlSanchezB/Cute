//////////////////////////////////////////////////////////////////////////
// Cute engine - Internal implementation of the render system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_SYSTEM_H_
#define RENDER_SYSTEM_H_

#include <memory>
#include <unordered_map>
#include <core/simple_pool.h>
#include <render/render_frame.h>
#include <job/job.h>
#include <core/platform.h>
#include <core/fast_map.h>

namespace render
{
	struct System;

	//Source of a resource
	enum class ResourceSource : uint8_t
	{
		Game,
		PassDescriptor,
		Pass
	};

	class RenderContextInternal : public RenderContext
	{
	public:
		RenderContextInternal(System* system, display::Device* device, const PassInfo& pass_info, ContextPass* _context_root_pass) :
			m_render_pass_system(system),
			m_display_device(device),
			m_pass_info(pass_info),
			m_context_root_pass(_context_root_pass)
		{
		}
		//Render pass system
		System* m_render_pass_system;
		//Root pass for the context
		ContextPass* m_context_root_pass = nullptr;
		//display context
		display::Context* m_display_context = nullptr;
		//device
		display::Device* m_display_device = nullptr;

		//Windows size
		PassInfo m_pass_info;

		//Point of view associated to this context
		PointOfView* m_point_of_view;
	};

	//Activated render contexts
	struct CachedRenderContext
	{
		uint16_t id;
		PassName pass_name;
		RenderContextInternal* render_context;
	};

	//Internal render pass system implementation
	struct System
	{
		//Display device
		display::Device* m_device;

		//Job system, it can be null
		job::System* m_job_system;

		//Platform game, it can be null. Used for present
		platform::Game* m_game;

		using ResourceFactoryMap = core::FastMap<RenderClassType, std::unique_ptr<FactoryInterface<Resource>>>;
		using PassFactoryMap = core::FastMap<RenderClassType, std::unique_ptr<FactoryInterface<Pass>>>;

		//Resource factories
		ResourceFactoryMap m_resource_factories_map;

		//Pass factories
		PassFactoryMap m_pass_factories_map;

		//Info for each resource
		struct ResourceInfo
		{
			//Resource
			std::unique_ptr<Resource> resource;

			//Source of the resource
			ResourceSource source;

			//State of the resource, used for syncing passes
			ResourceState state;

			ResourceInfo(std::unique_ptr<Resource>&_resource, const ResourceSource& _source) :
				resource(std::move(_resource)), source(_source), state("Init"_sh32)
			{
			}
		};

		//Resource info reference
		class ResourceInfoReference
		{
			ResourceName m_resource;

			//Cached pointer to fast access
			mutable ResourceInfo* m_resource_ptr = nullptr;
		public:
			ResourceInfoReference(ResourceName resource_name)
			{
				m_resource = resource_name;
			}
			void UpdateName(const ResourceName& resource_name)
			{
				m_resource = resource_name;
				m_resource_ptr = nullptr;
			}
			ResourceName GetResourceName() const
			{
				return m_resource;
			}
			ResourceInfo* Get(System* system) const
			{
				if (m_resource_ptr == nullptr)
				{
					m_resource_ptr = system->m_resources_map.Find(m_resource)->get();
				}
				return m_resource_ptr;
			}
		};

		using ResourceMap = core::FastMap<ResourceName, std::unique_ptr<ResourceInfo>>;
		using PassMap =core::FastMap<PassName, std::unique_ptr<Pass>>;

		//Gobal resources
		ResourceMap m_resources_map;

		//Passes defined in the passes declaration
		PassMap m_passes_map;

		//List of modules
		core::FastMap<ModuleName, std::unique_ptr<Module>> m_modules;

		//Buffer of all the render frame data
		//At the moment just one
		Frame m_frame_data;

		//Game thread frame, starts with 0 to sync with the display frame index
		uint64_t m_game_frame_index = 0;

		//Render thread frame, starts with 0 to sync with the display frame index
		uint64_t m_render_frame_index = 0;

		//List of activated render contexts, they get resused between frames using the pass name and id
		std::vector<CachedRenderContext> m_cached_render_context;

		//Vector of render priorities
		std::vector<PriorityName> m_render_priorities;

		//Render context created
		core::SimplePool<RenderContextInternal, 256> m_render_context_pool;

		//Render command list, used for render commands from the render system
		display::CommandListHandle m_render_command_list;

		//Create render context
		RenderContextInternal * CreateRenderContext(display::Device * device, const PassName& pass, const PassInfo& pass_info, std::vector<std::string>& errors);

		//Destroy render context
		void DestroyRenderContext(RenderContextInternal*& render_context);

		//Load from passes declaration file
		bool Load(LoadContext& load_context, const char* descriptor_file_buffer, size_t descriptor_file_buffer_size);

		//Add Resource
		bool AddResource(const ResourceName& name, std::unique_ptr<Resource>& resource, ResourceSource source);

		//Load resource
		ResourceName LoadResource(LoadContext& load_context, const char* prefix = nullptr);

		//Load Pass
		Pass* LoadPass(LoadContext& load_context);

		//Get Between frames cached render context
		RenderContextInternal* GetCachedRenderContext(const PassName& pass_name, uint16_t id, const PassInfo& pass_info);

		//Submit render
		void SubmitRender();
	};
}

#endif