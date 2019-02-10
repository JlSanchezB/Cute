//////////////////////////////////////////////////////////////////////////
// Cute engine - Manager of the render system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_H_
#define RENDER_H_

#include <render/render_common.h>
#include <render/render_frame.h>
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

	//Context used for rendering a pass
	//It will include all the information that the render pass manager needs for rendering a pass
	class RenderContext
	{
	public:
		//Add pass resource to this pass instance
		void AddPassResource(const ResourceName& name , std::unique_ptr<Resource>& resource);
		//Resource associated to this pass instance
		Resource* GetRenderResource(const ResourceName& name) const;

		template<typename RESOURCE>
		inline RESOURCE* GetResource(const ResourceName& name) const
		{
			Resource* resource = GetRenderResource(name);
			if (resource && resource->Type() == RESOURCE::kClassName)
			{
				return dynamic_cast<RESOURCE*>(resource);
			}
			return nullptr;
		}

		//Get Render frame
		Frame& GetRenderFrame();

		//Get root pass been rendering
		Pass* GetRootPass() const;

		//Get display device
		display::Device* GetDevice() const;

		//Get display Context
		display::Context* GetContext() const;

		struct PassInfo
		{
			uint32_t width;
			uint32_t height;
		};

		//Get pass info
		const PassInfo& GetPassInfo() const;

		//Set display context
		void SetContext(display::Context* context);

		//Update pass info
		void UpdatePassInfo(const PassInfo& pass_info);
	};

	//Create render system
	System* CreateRenderSystem();

	//Destroy render system
	void DestroyRenderSystem(System*& system, display::Device* device);

	//Load render pass descriptor file
	bool LoadPassDescriptorFile(System* system, display::Device* device, const char* descriptor_file_buffer, size_t descriptor_file_buffer_size, std::vector<std::string>& errors);

	//Add game resource, allows the game to add global resources that the pass system can access them
	bool AddGameResource(System* system, const ResourceName& name, std::unique_ptr<Resource> resource);

	//Register resource factory
	bool RegisterResourceFactory(System* system, const RenderClassType& resource_type, std::unique_ptr<FactoryInterface<Resource>>& resource_factory);

	//Register pass factory
	bool RegisterPassFactory(System* system, const RenderClassType& pass_type, std::unique_ptr<FactoryInterface<Pass>>& pass_factory);

	//Register resource factory helper
	template<typename RESOURCE>
	inline bool RegisterResourceFactory(System* system)
	{
		std::unique_ptr<FactoryInterface<Resource>> factory = std::make_unique<Factory<Resource, RESOURCE>>();
		return RegisterResourceFactory(system, RESOURCE::kClassName, factory);
	}

	//Register pass factory helper
	template<typename PASS>
	inline bool RegisterPassFactory(System* system)
	{
		std::unique_ptr<FactoryInterface<Pass>> factory = std::make_unique<Factory<Pass, PASS>>();
		return RegisterPassFactory(system, PASS::kClassName, factory);
	}

	//Get Resource by name
	Resource* GetResource(System* system, const ResourceName& name);

	template<typename RESOURCE>
	inline RESOURCE* GetResource(System* system, const ResourceName& name)
	{
		Resource* resource = GetResource(system, name);
		if (resource && resource->Type() == RESOURCE::kClassName)
		{
			return dynamic_cast<RESOURCE*>(resource);
		}
		return nullptr;
	}

	//Get Pass by name
	Pass* GetPass(System* system, const PassName& name);

	//Create a render context for rendering a pass
	using ResourceMap = std::unordered_map<ResourceName, std::unique_ptr<Resource>>;
	RenderContext* CreateRenderContext(System* system, display::Device* device, const PassName& pass, const RenderContext::PassInfo& pass_info, ResourceMap& init_resources, std::vector<std::string>& errors);

	//Destroy render context for rendering a pass
	void DestroyRenderContext(System* system, RenderContext*& render_context);

	//Capture render context
	void CaptureRenderContext(System* system, RenderContext* render_context);

	//Execute render context
	void ExecuteRenderContext(System* system, RenderContext* render_context);

	//Begin prepare render, the game thread can start to summit all the point of views and render items
	//Thread can be locked if there is not sufficient frame buffers
	void BeginPrepareRender(System* system);

	//End prepare, so game thread can jump to the next frame
	//The render is going to be submit to the GPU (on other thread)
	void EndPrepareRenderAndSubmit(System* system);

	//Get render frame for this frame
	//Only can be called from the game thread, between begin and end prepare frame
	Frame& GetGameRenderFrame(System* system);

	//Get the index of the priority for a priority name
	Priority GetRenderItemPriority(System* system, PriorityName priority_name);

}

#endif //RENDER_H_
