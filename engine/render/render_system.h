//////////////////////////////////////////////////////////////////////////
// Cute engine - Internal implementation of the render system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_SYSTEM_H_
#define RENDER_SYSTEM_H_

#include <memory>
#include <unordered_map>
#include <core/simple_pool.h>

namespace render
{
	struct System;

	class RenderContextInternal : public RenderContext
	{
	public:
		RenderContextInternal(System* system, ResourceMap& init_resources) : m_render_pass_system(system)
		{
			m_resources_map = std::move(init_resources);
		}
		//Resources associated to this pass
		ResourceMap m_resources_map;
		//Render pass system
		System* m_render_pass_system;
	};

	//Internal render pass system implementation
	struct System
	{
		//Load from passes declaration file
		bool Load(LoadContext& load_context);

		using ResourceFactoryMap = std::unordered_map<std::string, std::unique_ptr<FactoryInterface<Resource>>>;
		using PassFactoryMap = std::unordered_map<std::string, std::unique_ptr<FactoryInterface<Pass>>>;

		//Resource factories
		ResourceFactoryMap m_resource_factories_map;

		//Pass factories
		PassFactoryMap m_pass_factories_map;

		using ResourceMap = std::unordered_map<std::string, std::unique_ptr<Resource>>;
		using PassMap = std::unordered_map<std::string, std::unique_ptr<Pass>>;

		//Gobal resources defined in the passes declaration
		ResourceMap m_global_resources_map;

		//Game resource added by the game
		ResourceMap m_game_resources_map;

		//Passes defined in the passes declaration
		PassMap m_passes_map;

		//Render context created
		core::SimplePool<RenderContextInternal, 256> m_render_context_pool;

		//Load resource
		std::string LoadResource(LoadContext& load_context, const char* prefix = nullptr);

		//Get resource reference, it can be the name of the resource or the resource itself
		std::string GetResourceReference(LoadContext& load_context);

		//Load Pass
		Pass* LoadPass(LoadContext& load_context);
	};
}

#endif