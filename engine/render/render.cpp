#include "render.h"
#include <ext/tinyxml2/tinyxml2.h>
#include <core/log.h>
#include "render_helper.h"
#include "render_system.h"
#include "render_resource.h"
#include "render_pass.h"

#include <stdarg.h>

namespace
{
	template<class CONTAINER>
	void DestroyResources(display::Device* device, CONTAINER& container)
	{
		for (auto& item : container)
		{
			item.second->Destroy(device);
		}
		container.clear();
	}
}

namespace render
{
	void RenderContext::AddRenderResource(const char * name, std::unique_ptr<Resource>& resource)
	{
		auto render_context = reinterpret_cast<RenderContextInternal*>(this);
		render_context->m_resources_map[name] = std::move(resource);
	}

	Resource * RenderContext::GetRenderResource(const char * name) const
	{
		//First check context resources
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		const auto& it = render_context->m_resources_map.find(name);
		if (it != render_context->m_resources_map.end())
		{
			return it->second.get();
		}

		//Then check system resources
		return render::GetResource(render_context->m_render_pass_system, name);
	}

	Pass * RenderContext::GetRootPass() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_root_pass;
	}

	display::Device * RenderContext::GetDevice() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_display_device;
	}

	display::Context * RenderContext::GetContext() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_display_context;
	}

	const RenderContext::PassInfo& RenderContext::GetPassInfo() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_pass_info;
	}

	void RenderContext::SetContext(display::Context * context)
	{
		auto render_context = reinterpret_cast<RenderContextInternal*>(this);
		render_context->m_display_context = context;
	}

	void RenderContext::UpdatePassInfo(const PassInfo & pass_info)
	{
		auto render_context = reinterpret_cast<RenderContextInternal*>(this);
		render_context->m_pass_info = pass_info;
	}

	std::string System::LoadResource(LoadContext& load_context, const char* prefix)
	{
		//Get type and name
		const char* resource_type = load_context.current_xml_element->Attribute("type");
		const char* resource_name = load_context.current_xml_element->Attribute("name");
		if (resource_type && resource_name)
		{
			auto& resource_factory_it = m_resource_factories_map.find(resource_type);
			if (resource_factory_it != m_resource_factories_map.end())
			{
				if (m_global_resources_map.find(resource_name) == m_global_resources_map.end())
				{
					auto& factory = resource_factory_it->second;

					assert(factory.get());

					//Create resource container
					auto resource_instance = factory->Create();

					assert(resource_instance);
					assert(strcmp(resource_instance->Type(), resource_type) == 0);

					load_context.name = resource_name;

					//Load resource
					resource_instance->Load(load_context);

					core::LogInfo("Created Resource <%s> type <%s>", resource_name, resource_type);

					//Add to the globals
					if (prefix)
					{
						std::string name = std::string(prefix) + resource_name;
						m_global_resources_map[name].reset(resource_instance);
						return name;
					}
					else
					{
						m_global_resources_map[resource_name].reset(resource_instance);
						return resource_name;
					}
				}
				else
				{
					AddError(load_context, "Resource name <%s> has been already added", resource_name);
				}
			}
			else
			{
				AddError(load_context, "Resource type <%s> is not register", resource_type);
			}
		}
		else
		{
			AddError(load_context, "Resource has not attribute type or name");
		}
		return std::string();
	}

	Pass* System::LoadPass(LoadContext& load_context)
	{
		//Create the pass
		const char* pass_type = load_context.current_xml_element->Name();
		const char* pass_name = load_context.current_xml_element->Attribute("name");

		
		auto& pass_factory_it = m_pass_factories_map.find(pass_type);
		if (pass_factory_it != m_pass_factories_map.end())
		{
			//Load the pass
			auto& factory = pass_factory_it->second;

			assert(factory.get());

			//Create pass instance
			auto pass_instance = factory->Create();

			assert(pass_instance);
			assert(strcmp(pass_instance->Type(), pass_type) == 0);

			load_context.name = pass_type;

			//Load pass
			pass_instance->Load(load_context);

			return pass_instance;
		}
		else
		{
			AddError(load_context, "Pass type <%s> is not register", pass_type);
			return nullptr;
		}
	}

	bool System::Load(LoadContext& load_context)
	{
		tinyxml2::XMLDocument xml_doc;

		tinyxml2::XMLError result = xml_doc.LoadFile(load_context.render_passes_filename);
		if (result != tinyxml2::XML_SUCCESS)
		{
			AddError(load_context, "File <%s> doesn't exist", load_context.render_passes_filename);
			return false;
		}

		tinyxml2::XMLNode* root = xml_doc.FirstChildElement("Root");
		if (root == nullptr)
		{
			AddError(load_context, "Root node doesn't exist");
			return false;
		}

		//Set the xml doc to the load context
		load_context.xml_doc = &xml_doc;

		//Load global resources
		tinyxml2::XMLElement* global = root->FirstChildElement("Global");
		if (global)
		{
			tinyxml2::XMLElement* resource = global->FirstChildElement();
			while (resource)
			{
				if (strcmp(resource->Name(), "Resource") == 0)
				{
					//It is a resource
					load_context.current_xml_element = resource;

					LoadResource(load_context);
				}
				else
				{
					AddError(load_context, "Global element <%s> not supported", resource->Name());
				}

				resource = resource->NextSiblingElement();
			}
		}

		//Load Passes
		tinyxml2::XMLElement* passes_element = root->FirstChildElement("Passes");
		if (passes_element)
		{
			tinyxml2::XMLElement* pass_element = passes_element->FirstChildElement();
			while (pass_element)
			{
				if (CheckNodeName(pass_element, "Pass"))
				{
					const char* pass_name = pass_element->Attribute("name");
					if (pass_name)
					{
						auto pass_it = m_passes_map.find(pass_name);
						if (pass_it == m_passes_map.end())
						{
							load_context.current_xml_element = pass_element;
							load_context.name = pass_name;
							load_context.pass_name = pass_name;

							//It is a root pass (usually a context pass), must have name so the game can find it

							Pass* pass = LoadPass(load_context);

							//Add it to the pass map
							m_passes_map[pass_name].reset(pass);

							core::LogInfo("Created Pass <%s>", pass_name);
						}
						else
						{
							AddError(load_context, "Pass <%s> already exist, discarting new one", pass_name);
						}
					}
					else
					{
						AddError(load_context, "Pass inside the node <Passes> must have name attribute");
					}
				}
				else
				{
					AddError(load_context, "Only nodes <Pass> are supported inside the node <Passes>");
				}

				pass_element = pass_element->NextSiblingElement();
			}
		}
		return (load_context.errors.size() == 0);
	}


	System * CreateRenderPassSystem()
	{
		System* system = new System();

		//Register all basic resources factories
		RegisterResourceFactory<BoolResource>(system);
		RegisterResourceFactory<TextureResource>(system);
		RegisterResourceFactory<ConstantBufferResource>(system);
		RegisterResourceFactory<VertexBufferResource>(system);
		RegisterResourceFactory<RenderTargetResource>(system);
		RegisterResourceFactory<RenderTargetReferenceResource>(system);
		RegisterResourceFactory<RootSignatureResource>(system);
		RegisterResourceFactory<GraphicsPipelineStateResource>(system);
		RegisterResourceFactory<ComputePipelineStateResource>(system);
		RegisterResourceFactory<DescriptorTableResource>(system);

		//Register all basic passes factories
		RegisterPassFactory<ContextPass>(system);
		RegisterPassFactory<SetRenderTargetPass>(system);
		RegisterPassFactory<ClearRenderTargetPass>(system);
		RegisterPassFactory<SetRootSignaturePass>(system);
		RegisterPassFactory<SetPipelineStatePass>(system);
		RegisterPassFactory<SetDescriptorTablePass>(system);
		RegisterPassFactory<DrawFullScreenQuadPass>(system);

		return system;
	}

	void DestroyRenderPassSystem(System *& system, display::Device* device)
	{
		//Destroy resources and passes
		DestroyResources(device, system->m_game_resources_map);
		DestroyResources(device, system->m_global_resources_map);
		DestroyResources(device, system->m_passes_map);

		delete system;

		system = nullptr;
	}

	bool LoadPassDescriptorFile(System* system, display::Device* device, const char * pass_descriptor_file, std::vector<std::string>& errors)
	{
		LoadContext load_context;
		load_context.device = device;
		load_context.render_passes_filename = pass_descriptor_file;
		load_context.render_system = system;

		bool success = system->Load(load_context);

		if (!success)
		{
			//Log the errors
			core::LogError("Errors loading render pass descriptor file <%s>:", pass_descriptor_file);

			for (auto& error : load_context.errors)
			{
				core::LogError(error.c_str());
			}

			errors = std::move(load_context.errors);

			//Clear all resources created from the file
			DestroyResources(device, system->m_global_resources_map);
			DestroyResources(device, system->m_passes_map);
		}
		else
		{
			core::LogInfo("Render pass descriptor file <%s> loaded", pass_descriptor_file);
		}
		return success;
	}

	RenderContext * CreateRenderContext(System * system, display::Device * device, const char * pass, const RenderContext::PassInfo& pass_info, ResourceMap& init_resources, std::vector<std::string>& errors)
	{
		//Get pass
		auto render_pass = GetPass(system, pass);
		if (render_pass)
		{
			//Create Render Context
			RenderContextInternal* render_context = system->m_render_context_pool.Alloc(system, device, pass_info, init_resources, render_pass);

			ErrorContext errors_context;

			//Allow the passes to init the render context 
			render_pass->InitPass(*render_context, device, errors_context);

			errors = std::move(errors_context.errors);

			if (errors.empty())
			{
				core::LogInfo("Created a render pass from definition pass <%s>", pass);
				return render_context;
			}
			else
			{
				core::LogError("Errors creating a render pass from definition pass <%s>:", pass);
				for (auto& error : errors)
				{
					core::LogError(error.c_str());
				}

				RenderContext * render_context_ref = reinterpret_cast<RenderContext*>(render_context);
				DestroyRenderContext(system, render_context_ref);
				return nullptr;
			}
			
		}
		else
		{
			errors.push_back(std::string("Pass <") + pass + "not found");
			core::LogError("Errors creating a render pass, definition pass <%s> doesn't exist", pass);
			return nullptr;
		}
	}

	void DestroyRenderContext(System * system, RenderContext*& render_context)
	{
		auto render_context_internal = reinterpret_cast<RenderContextInternal*>(render_context);
		//Destroy context resources
		DestroyResources(render_context_internal->m_display_device, render_context_internal->m_resources_map);

		system->m_render_context_pool.Free(render_context_internal);
		
		render_context = nullptr;
	}

	void CaptureRenderContext(System * system, RenderContext * render_context)
	{
		auto render_context_internal = reinterpret_cast<RenderContextInternal*>(render_context);

		//Open and capture all command list in the render context
		render_context_internal->m_root_pass->Render(*render_context);
	}

	void ExecuteRenderContext(System * system, RenderContext * render_context)
	{
		auto render_context_internal = reinterpret_cast<RenderContextInternal*>(render_context);
		//Open and capture all command list in the render context

		render_context_internal->m_root_pass->Execute(*render_context);
	}

	bool AddGameResource(System * system, const char * name, std::unique_ptr<Resource>& resource)
	{
		if ((system->m_global_resources_map.find(name) != system->m_global_resources_map.end()) ||
			(system->m_game_resources_map.find(name) != system->m_game_resources_map.end()))
		{
			system->m_game_resources_map[name] = std::move(resource);
			return true;
		}
		else
		{
			core::LogWarning("Game Resource <%s> has been already added, discarting the new resource");
			return false;
		}
	}

	bool RegisterResourceFactory(System * system, const char * resource_type, std::unique_ptr<FactoryInterface<Resource>>& resource_factory)
	{
		if (system->m_resource_factories_map.find(resource_type) != system->m_resource_factories_map.end())
		{
			core::LogWarning("Resource <%s> has been already added, discarting new resource type", resource_type);
			return false;
		}
		system->m_resource_factories_map[resource_type] = std::move(resource_factory);
		return true;
	}

	bool RegisterPassFactory(System * system, const char * pass_type, std::unique_ptr<FactoryInterface<Pass>>& pass_factory)
	{
		if (system->m_resource_factories_map.find(pass_type) != system->m_resource_factories_map.end())
		{
			core::LogWarning("Pass <%s> has been already added, discarting new pass type", pass_type);
			return false;
		}

		system->m_pass_factories_map[pass_type] = std::move(pass_factory);
		return true;
	}

	Resource * GetResource(System* system, const char * name)
	{
		auto it_game = system->m_game_resources_map.find(name);
		if (it_game != system->m_game_resources_map.end())
		{
			return it_game->second.get();
		}

		auto it_global = system->m_global_resources_map.find(name);
		if (it_global != system->m_global_resources_map.end())
		{
			return it_global->second.get();
		}
		return nullptr;
	}

	Pass * GetPass(System* system, const char * name)
	{
		auto it = system->m_passes_map.find(name);
		if (it != system->m_passes_map.end())
		{
			return it->second.get();
		}
		return nullptr;
	}

	std::string LoadContext::GetResourceReference(LoadContext & load_context)
	{
		//Check if is a resource
		if (auto xml_resource_element = load_context.current_xml_element->FirstChildElement("Resource"))
		{
			//It is a resource, load it using as prefix the pass name and return the name 
			load_context.current_xml_element = xml_resource_element;
			return load_context.render_system->LoadResource(load_context, load_context.pass_name);
		}
		else
		{
			//The resource is in the value
			return std::string(load_context.current_xml_element->GetText());
		}
	}

	bool LoadContext::AddResource(const char * name, std::unique_ptr<Resource>& resource)
	{
		if ((render_system->m_global_resources_map.find(name) == render_system->m_global_resources_map.end()) &&
			(render_system->m_game_resources_map.find(name) == render_system->m_game_resources_map.end()))
		{
			render_system->m_global_resources_map[name] = std::move(resource);
			return true;
		}
		else
		{
			resource.release();
			core::LogWarning("Global Resource <%s> has been already added, discarting the new resource");
			return false;
		}
	}
}