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
	//System
	struct System;

	//Context used for loading a pass
	struct LoadContext
	{
		display::Device* device;
		tinyxml2::XMLDocument* xml_doc;
		tinyxml2::XMLElement* current_xml_element;
		const char* render_passes_filename;
		std::vector<std::string> errors;
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

	//Create render pass system
	System* CreateRenderPassSystem();

	//Destroy render pass system
	void DestroyRenderPassSystem(System* system);

	//Load render pass descriptor file
	bool LoadPassDescriptorFile(System* system, display::Device* device, const char* pass_descriptor_file, std::vector<std::string>& errors);

	//Register resource factory
	bool RegisterResourceFactory(System* system, const char * resource_type, std::unique_ptr<FactoryInterface<Resource>> resource_factory);

	//Register pass factory
	bool RegisterPassFactory(System* system, const char * pass_type, std::unique_ptr<FactoryInterface<Pass>> pass_factory);

	//Register resource factory helper
	template<typename RESOURCE>
	inline bool RegisterResourceFactory(System* system, const char * type)
	{
		return RegisterResourceFactory(System* system, type, std::make_unique<Factory<Resource, RESOURCE>>());
	}

	//Register pass factory helper
	template<typename PASS>
	inline bool RegisterPassType(const char * type)
	{
		return RegisterResourceFactory(System* system, type, std::make_unique<Factory<Pass, PASS>>());
	}
}

#endif //RENDER_MANAGER_H_
