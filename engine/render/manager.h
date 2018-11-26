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
		virtual ~Resource() {};
		//Load from XML node and returns the Resource
		virtual Resource* Load() = 0;
		//Returns the type of resource
		virtual const char* Type() = 0;
	};
	
	//Base Pass class
	class Pass
	{
	public:
		virtual ~Pass() {};
		//Load from XML node and returns the Resource
		virtual Pass* Load() = 0;
		//Render the pass
		virtual void Render(RenderContext* render_context) = 0;
	};

	//Factory helper classes
	template<class TYPE>
	class FactoryInterface
	{
	public:
		virtual TYPE* Create() = 0;
	};
	template<class TYPE, class RESOURCE>
	class ResourceFactory : public FactoryInterface<TYPE>
	{
	public:
		TYPE* Create() override
		{
			return new RESOURCE();
		}
	};


	//Render pass manager
	class Manager
	{
	public:
		//Register resource factory
		template<typename RESOURCE>
		void RegisterResourceFactory(const char* type)
		{
			m_resource_factories_map[type] = std::make_unsigned_t<Factory<Resource, RESOURCE>>();
		}
		
		//Register pass factory
		template<typename PASS>
		void RegisterPassType(const char* type)
		{
			m_pass_factories_map[type] = std::make_unsigned_t<Factory<Pass, PASS>>();
		}

		//Load from passes declaration file
		void Load();

	private:
		using ResourceFactoryMap = std::unordered_map<const char*, std::unique_ptr<FactoryInterface<Resource>*>>;
		using PassFactoryMap = std::unordered_map<const char*, std::unique_ptr<FactoryInterface<Pass>*>>;

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
