#include "render.h"
#include <ext/tinyxml2/tinyxml2.h>
#include "render_resource.h"
#include <core/log.h>
#include "render_helper.h"

#include <memory>
#include <unordered_map>
#include <stdarg.h>

namespace render
{
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

		//Load resource
		void LoadResource(LoadContext& load_context);
	};

	void System::LoadResource(LoadContext& load_context)
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

					//Add to the globals
					m_global_resources_map[resource_name].reset(resource_instance);
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

		return (load_context.errors.size() == 0);
	}


	System * CreateRenderPassSystem()
	{
		System* system = new System();

		//Register all basic resources factories and passes
		RegisterResourceFactory<BoolResource>(system);
		RegisterResourceFactory<TextureResource>(system);
		RegisterResourceFactory<ConstantBufferResource>(system);
		RegisterResourceFactory<RootSignatureResource>(system);

		return system;
	}

	void DestroyRenderPassSystem(System * system)
	{
		delete system;
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
		}
		return success;
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

}