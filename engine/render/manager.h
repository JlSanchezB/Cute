//////////////////////////////////////////////////////////////////////////
// Cute engine - Manager of the render passes system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_MANAGER_H_
#define RENDER_MANAGER_H_

#include <memory>
#include <unordered_map>
#include <core/log.h>

namespace display
{
	struct Device;
	struct Context;
}

namespace tinyxml2
{
	class XMLDocument;
	class XMLElement;
}

namespace render
{
	//Context used for loading a pass
	struct LoadContext
	{
		display::Device* device;
		tinyxml2::XMLDocument* xml_doc;
		tinyxml2::XMLElement* current_xml_element;
		const char* render_passes_filename;
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
		virtual Resource* Load(LoadContext* load_context) = 0;
		//Returns the type of resource
		virtual const char* Type() = 0;
	};
	
	//Base Pass class
	class Pass
	{
	public:
		virtual ~Pass() {};
		//Load from XML node and returns the Resource
		virtual Pass* Load(LoadContext* load_context) = 0;
		//Render the pass
		virtual void Render(RenderContext* render_context) = 0;
	};

	//Factory helper classes
	template<class TYPE>
	class FactoryInterface
	{
	public:
		virtual std::unique_ptr<TYPE> Create() = 0;
	};
	template<class TYPE, class RESOURCE>
	class ResourceFactory : public FactoryInterface<TYPE>
	{
	public:
		std::unique_ptr<TYPE> Create() override
		{
			return std::make_unique<RESOURCE>();
		}
	};


	//Render pass manager
	class Manager
	{
	public:
		//Register resource factory
		template<typename RESOURCE>
		void RegisterResourceFactory(const char* type);
		
		//Register pass factory
		template<typename PASS>
		void RegisterPassType(const char* type);

		//Load from passes declaration file
		bool Load(display::Device* device, const char* render_passes_declaration);

	private:
		using ResourceFactoryMap = std::unordered_map<const char*, std::unique_ptr<FactoryInterface<Resource>>>;
		using PassFactoryMap = std::unordered_map<const char*, std::unique_ptr<FactoryInterface<Pass>>>;

		//Resource factories
		ResourceFactoryMap m_resource_factories_map;

		//Pass factories
		PassFactoryMap m_pass_factories_map;

		using ResourceMap = std::unordered_map<const char*, std::unique_ptr<Resource>>;
		using PassMap = std::unordered_map<const char*, std::unique_ptr<Pass>>;

		//Gobal resources defined in the passes declaration
		ResourceMap m_global_resources_map;

		//Passes defined in the passes declaration
		PassMap m_passes_map;

		//Load resource
		void LoadResource(render::LoadContext &load_context);
	};

	//Register resource factory

	template<typename RESOURCE>
	inline void Manager::RegisterResourceFactory(const char * type)
	{
		if (m_resource_factories_map.find(type) != m_resource_factories_map.end())
		{
			core::LogWarning("Resource <%s> has been already added, discarting new resource type", type);
			return;
		}
		m_resource_factories_map[type] = std::make_unsigned_t<Factory<Resource, RESOURCE>>();
	}

	//Register pass factory

	template<typename PASS>
	inline void Manager::RegisterPassType(const char * type)
	{
		if (m_resource_factories_map.find(type) != m_resource_factories_map.end())
		{
			core::LogWarning("Pass <%s> has been already added, discarting new pass type", type);
			return;
		}
		m_pass_factories_map[type] = std::make_unsigned_t<Factory<Pass, PASS>>();
	}
}

#endif //RENDER_MANAGER_H_
