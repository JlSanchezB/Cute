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

namespace render
{
	struct System;

	//Sorted render items
	struct SortedRenderItems
	{
		//Sorted render items list
		std::vector<Item> m_sorted_render_items;
		//Index access to the sorted render items by priority (begin item and end item)
		std::vector<std::pair<size_t, size_t>> m_priority_table;
	};

	class RenderContextInternal : public RenderContext
	{
	public:
		RenderContextInternal(System* system, display::Device* device, const PassInfo& pass_info, ResourceMap& init_resources, Pass* _root_pass) :
			m_render_pass_system(system),
			m_display_device(device),
			m_pass_info(pass_info)
		{
			m_root_pass = _root_pass;
			m_game_resources_map = std::move(init_resources);
		}
		//Game resources associated to this pass
		ResourceMap m_game_resources_map;
		//Pass resources associated to this pass
		ResourceMap m_pass_resources_map;
		//Render pass system
		System* m_render_pass_system;
		//Root pass for the cotnext
		Pass* m_root_pass = nullptr;
		//display context
		display::Context* m_display_context = nullptr;
		//device
		display::Device* m_display_device = nullptr;

		//Windows size
		PassInfo m_pass_info;

		//Sorted render items associated to this context
		SortedRenderItems m_render_items;

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

		using ResourceFactoryMap = std::unordered_map<RenderClassType, std::unique_ptr<FactoryInterface<Resource>>>;
		using PassFactoryMap = std::unordered_map<RenderClassType, std::unique_ptr<FactoryInterface<Pass>>>;

		//Resource factories
		ResourceFactoryMap m_resource_factories_map;

		//Pass factories
		PassFactoryMap m_pass_factories_map;

		using ResourceMap = std::unordered_map<ResourceName, std::unique_ptr<Resource>>;
		using PassMap = std::unordered_map<PassName, std::unique_ptr<Pass>>;

		//Gobal resources defined in the passes declaration
		ResourceMap m_global_resources_map;

		//Game resource added by the game
		ResourceMap m_game_resources_map;

		//Passes defined in the passes declaration
		PassMap m_passes_map;

		//Buffer of all the render frame data
		//At the moment just one
		Frame m_frame_data;

		//Game thread frame
		size_t m_game_thread_frame = 0;

		//Render thread frame
		size_t m_render_thread_frame = 0;

		//List of activated render contexts, they get resused between frames using the pass name and id
		std::vector<CachedRenderContext> m_cached_render_context;

		//Vector of render priorities
		std::vector<PriorityName> m_render_priorities;

		//Render context created
		core::SimplePool<RenderContextInternal, 256> m_render_context_pool;

		//Render command list, used for render commands from the render system
		display::CommandListHandle m_render_command_list;

		//Create render context
		RenderContextInternal * CreateRenderContext(display::Device * device, const PassName& pass, const PassInfo& pass_info, ResourceMap& init_resources, std::vector<std::string>& errors);

		//Destroy render context
		void DestroyRenderContext(RenderContextInternal*& render_context);

		//Load from passes declaration file
		bool Load(LoadContext& load_context, const char* descriptor_file_buffer, size_t descriptor_file_buffer_size);

		//Load resource
		ResourceName LoadResource(LoadContext& load_context, const char* prefix = nullptr);

		//Load Pass
		Pass* LoadPass(LoadContext& load_context);

		//Get Between frames cached render context
		RenderContextInternal* GetCachedRenderContext(const PassName& pass_name, uint16_t id, const PassInfo& pass_info, ResourceMap& init_resource_map);

		//Submit render
		void SubmitRender();
	};
}

#endif