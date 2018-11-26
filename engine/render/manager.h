//////////////////////////////////////////////////////////////////////////
// Cute engine - Manager of the render passes system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_MANAGER_H_
#define RENDER_MANAGER_H_

#include <memory>
#include <unordered_map>

namespace render
{
	//Context used for loading a pass
	class LoadContext
	{
	public:
	};

	//Context used for rendering a pass
	//It will include all the information that the render pass manager needs for rendering a pass
	class RenderContext
	{
	public:

	};

	//Base resource class
	class Resource
	{
	public:
		~Resource();

		//Returns the type of resource
		virtual const char* Type() = 0;
	};

	//Resource Type Processor
	class ResourceFactory
	{
	public:
		//Load from XML node and returns the Resource
		virtual Resource* Load() = 0;
	};
	
	//Base Pass class
	class Pass
	{
	public:
		~Pass();

		//Render the pass
		virtual void Render(RenderContext* render_context) = 0;
	};

	//Pass factor
	class PassFactory
	{
	public:
		//Load from XML node and returns the Resource
		virtual Pass* Load() = 0;
	};

	


	//Render pass manager
	class Manager
	{
	public:
		//Register resource factory
		template<typename RESOURCE_FACTORY>
		void RegisterResourceFactory(const char* type)
		{
			m_resource_factories_map[type] = std::make_unsigned_t<RESOURCE_FACTORY>();
		}
		
		//Register pass factory
		template<typename PASS_FACTORY>
		void RegisterPassType(const char* type)
		{
			m_pass_factories_map[type] = std::make_unsigned_t<PASS_FACTORY>();
		}

		//Load from passes declaration file
		void Load();

	private:
		using ResourceFactoryMap = std::unordered_map<const char*, std::unique_ptr<ResourceFactory*>>;
		using PassFactoryMap = std::unordered_map<const char*, std::unique_ptr<PassFactory*>>;

		//Resource factories
		ResourceFactoryMap m_resource_factories_map;

		//Pass factories
		PassFactoryMap m_pass_factories_map;

		using ResourceMap = std::unordered_map<const char*, std::unique_ptr<Resource*>>;
		using PassMap = std::unordered_map<const char*, std::unique_ptr<Pass*>>;

		//Gobal resources defined in the passes declaration
		ResourceMap m_global_resources_map;

		//Passes defined in the passes declaration
		PassMap m_passes_map;
	};
}

#endif //RENDER_MANAGER_H_
