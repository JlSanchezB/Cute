//////////////////////////////////////////////////////////////////////////
// Cute engine - Common definition shared in the render system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_COMMON_H_
#define RENDER_COMMON_H_

#include <core/string_hash.h>
#include <core/fast_map.h>
#include <vector>
#include <memory>
#include <unordered_map>

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
	const RenderClassType Type() const override\
	{ \
		return RenderClassType(name); \
	}; \
	inline static const RenderClassType kClassName = RenderClassType(name);

namespace render
{
	using RenderClassType = StringHash32<"RenderClassType"_namespace>;
	using ResourceName = StringHash32<"ResourceName"_namespace>;
	using PassName = StringHash32<"PassName"_namespace>;
	using PriorityName = StringHash32<"PriorityName"_namespace>;
	using Priority = uint8_t;
	using SortKey = uint32_t;

	//Information needed for each render pass
	struct PassInfo
	{
		uint32_t width;
		uint32_t height;
	};

	//System
	struct System;
	class RenderContext;
	class Resource;

	struct ErrorContext
	{
		std::vector<std::string> errors;
	};

	//Context used for loading a pass
	struct LoadContext : ErrorContext
	{
		display::Device* device;
		tinyxml2::XMLDocument* xml_doc;
		tinyxml2::XMLElement* current_xml_element;
		const char* pass_name;
		const char* name;
		render::System* render_system;

		//Get resource reference, it can be the name of the resource or the resource itself
		ResourceName GetResourceReference(LoadContext& load_context);

		//Add resource
		bool AddResource(const ResourceName& name, std::unique_ptr<Resource>& resource);
	};
	//Base resource class
	class Resource
	{
	public:
		virtual ~Resource() {};
		//Load from XML node
		virtual void Load(LoadContext& load_context) = 0;
		//Destroy device handles
		virtual void Destroy(display::Device* device) {};

		//Return type, it will be defined with DECLARE_RENDER_CLASS
		virtual const RenderClassType Type() const = 0;
	};

	//Base Pass class
	class Pass
	{
	public:
		virtual ~Pass() {};
		//Load from XML node and returns the Resource
		virtual void Load(LoadContext& load_context) = 0;
		//Destroy device handles
		virtual void Destroy(display::Device* device) {};

		//Init pass, called when a render context is created for this pass
		virtual void InitPass(RenderContext& render_context, display::Device* device, ErrorContext& errors) {};

		//Render the pass, capture all command list
		virtual void Render(RenderContext& render_context) const {};

		//Execute all command list
		virtual void Execute(RenderContext& render_context) const {};

		//Return type, it will be defined with DECLARE_RENDER_CLASS
		virtual const RenderClassType Type() const = 0;
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
			return dynamic_cast<TYPE*> (new SPECIALISED());
		}
	};

	using ResourceMap = core::FastMap<ResourceName, std::unique_ptr<Resource>>;
}


#endif //RENDER_COMMON_H_
