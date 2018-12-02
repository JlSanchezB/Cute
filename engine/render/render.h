//////////////////////////////////////////////////////////////////////////
// Cute engine - Manager of the render passes system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_H_
#define RENDER_H_

#include <vector>
#include <memory>

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

#define DECLARE_RENDER_CLASS(name) \
	const char* Type() const \
	{ \
		return name; \
	}; \
	inline static const char* kClassName = name;

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
		const char* name;
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
		virtual void Load(LoadContext& load_context) = 0;
		
		//Return type, it will be defined with DECLARE_RENDER_CLASS
		virtual const char* Type() const = 0;
	};
	
	//Base Pass class
	class Pass
	{
	public:
		virtual ~Pass() {};
		//Load from XML node and returns the Resource
		virtual void Load(LoadContext* load_context) = 0;
		//Render the pass
		virtual void Render(RenderContext* render_context) const = 0;
	};

	//Factory helper classes
	template<class TYPE>
	struct FactoryInterface
	{
		virtual TYPE* Create() = 0;
	};
	template<class TYPE, class SPECIALISED>
	struct Factory : FactoryInterface<TYPE>
	{
		TYPE* Create() override
		{
			return reinterpret_cast<TYPE*> (new SPECIALISED());
		}
	};

	//Create render pass system
	System* CreateRenderPassSystem();

	//Destroy render pass system
	void DestroyRenderPassSystem(System* system);

	//Load render pass descriptor file
	bool LoadPassDescriptorFile(System* system, display::Device* device, const char* pass_descriptor_file, std::vector<std::string>& errors);

	//Add global resource, allows the game to add global resources that the pass system can access them
	bool AddGameResource(System* system, const char* name, std::unique_ptr<Resource>& resource);

	//Register resource factory
	bool RegisterResourceFactory(System* system, const char * resource_type, std::unique_ptr<FactoryInterface<Resource>>& resource_factory);

	//Register pass factory
	bool RegisterPassFactory(System* system, const char * pass_type, std::unique_ptr<FactoryInterface<Pass>>& pass_factory);

	//Register resource factory helper
	template<typename RESOURCE>
	inline bool RegisterResourceFactory(System* system)
	{
		std::unique_ptr<FactoryInterface<Resource>> factory = std::make_unique<Factory<Resource, RESOURCE>>();
		return RegisterResourceFactory(system, RESOURCE::kClassName, factory);
	}

	//Register pass factory helper
	template<typename PASS>
	inline bool RegisterPassType(System* system)
	{
		std::unique_ptr<FactoryInterface<Pass>> factory = std::make_unique<Factory<Pass, PASS>>();
		return RegisterResourceFactory(system, PASS::kClassName, factory);
	}
}

#endif //RENDER_H_
