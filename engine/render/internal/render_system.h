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
		Game, //Resource added from the game, it can be access by name
		PassDescriptor, //Resource is defined where it is used, it doesn't have name, it is an inline resource declared in pass declaration
		Pass, //Resource is created by the pass itself, resources that are different by pass (id and passname). For example a resource different by view.
		Pool //Resource that is allocated, used and freed during the rendering, it can be used by different passes and reused as well. The resource has a timeline inside the rendering.
	};

	enum class PoolResourceType
	{
		RenderTarget,
		DepthBuffer
	};

	class RenderContextInternal : public RenderContext
	{
	public:
		RenderContextInternal(System* system, display::Device* device, const PassName& pass_name, const uint16_t pass_id, const PassInfo& pass_info, ContextPass* _context_root_pass) :
			m_render_pass_system(system),
			m_display_device(device),
			m_pass_name(pass_name),
			m_pass_id(pass_id),
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
		PointOfView* m_point_of_view = nullptr;

		//Pass name
		PassName m_pass_name;

		//Pass id
		uint16_t m_pass_id;
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

		//Job allocator
		std::unique_ptr<job::JobAllocator<1024 * 1024>> m_job_allocator;

		//Info for each resource
		struct ResourceInfo
		{
			//Resource
			std::unique_ptr<Resource> resource;

			//Source of the resource
			ResourceSource source;

			//State of the resource, used for syncing passes
			ResourceState state;

			//Current access of the resource
			display::TranstitionState access;

			ResourceInfo(std::unique_ptr<Resource>&_resource, const ResourceSource& _source, const display::TranstitionState& _access) :
				resource(std::move(_resource)), source(_source), state("Init"_sh32), access(_access)
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
					ResourceInfo*  resource_ptr = system->m_resources_map.Find(m_resource)->get();

					//Check if it can be cached
					if (resource_ptr->source != ResourceSource::Pool &&
						resource_ptr->source != ResourceSource::Pass)
					{
						m_resource_ptr = resource_ptr;
						return resource_ptr;
					}
					else
					{
						return resource_ptr;
					}
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
		//Two buffers, one in the render and other in the game, that allows to render and prepare the next frame at the same time
		Frame m_frame_data[2];

		//Game thread frame, starts with 1 to sync with the display frame index (display frames start with 1, that allows zero as non frame rendered)
		uint64_t m_game_frame_index = 1;

		//Render thread frame, starts with 1 to sync with the display frame index
		uint64_t m_render_frame_index = 1;

		//List of activated render contexts, they get resused between frames using the pass name and id
		std::vector<CachedRenderContext> m_cached_render_context;

		//Vector of render priorities
		std::vector<PriorityName> m_render_priorities;

		//Render context created
		core::SimplePool<RenderContextInternal, 256> m_render_context_pool;

		//Render command list, used for render commands from the render system
		display::CommandListHandle m_render_command_list;

		//Pool resource
		struct PoolResource
		{
			std::unique_ptr<Resource> resource;
			ResourceName name;
			PoolResourceType type;
			uint16_t width;
			uint16_t height;
			display::Format format;
			bool can_be_reuse;
			uint64_t last_render_frame_used = 0;
			display::TranstitionState m_access;
		};
		//Container for the pool resources
		std::vector<PoolResource> m_pool_resources;

		//Create render context
		RenderContextInternal * CreateRenderContext(display::Device * device, const PassName& pass, const uint16_t pass_id, const PassInfo& pass_info, std::vector<std::string>& errors);

		//Destroy render context
		void DestroyRenderContext(RenderContextInternal*& render_context);

		//Load from passes declaration file
		bool Load(LoadContext& load_context, const char* descriptor_file_buffer, size_t descriptor_file_buffer_size);

		//Add Resource
		bool AddResource(const ResourceName& name, std::unique_ptr<Resource>&& resource, ResourceSource source, const std::optional<display::TranstitionState>& current_access = {});

		//Get Resource
		Resource* GetResource(const ResourceName& name, ResourceSource& source);

		//Load resource
		ResourceName LoadResource(LoadContext& load_context, const char* prefix = nullptr);

		//Load Pass
		Pass* LoadPass(LoadContext& load_context);

		//Alloc pool resource
		std::pair<std::unique_ptr<Resource>, display::TranstitionState> AllocPoolResource(ResourceName resource_name, PoolResourceType type, uint16_t width, uint16_t weight, const display::Format& format);

		//Dealloc pool resource
		void DeallocPoolResource(ResourceName resource_name, std::unique_ptr<Resource>& resource, const display::TranstitionState state);

		//Update pool resources (free resources that have not used for two frames)
		void UpdatePoolResources();

		//Get Between frames cached render context
		RenderContextInternal* GetCachedRenderContext(const PassName& pass_name, uint16_t id, const PassInfo& pass_info);

		//Submit render
		void SubmitRender();
	};
}

#endif