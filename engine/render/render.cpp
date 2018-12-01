#include "render.h"
#include <ext/tinyxml2/tinyxml2.h>

namespace render
{
	//Internal render pass system implementation
	struct System
	{
		//Load from passes declaration file
		bool Load(LoadContext& load_context);

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

					//Create resource container
					auto resource_instance = factory->Create();

					//Load resource
					resource_instance->Load(&load_context);

					//Add to the globals
					m_global_resources_map[resource_name] = std::move(resource_instance);
				}
				else
				{
					core::LogError("Error loading <%s> render passes declaration, resource name <%s> has been already added", load_context.render_passes_filename, resource_name);
				}
			}
			else
			{
				core::LogError("Error loading <%s> render passes declaration, resource type <%s> is not register", load_context.render_passes_filename, resource_type);
			}
		}
		else
		{
			core::LogError("Error loading <%s> render passes declaration, resource has not attribute type or name");
		}
	}

	bool System::Load(LoadContext& load_context)
	{
		tinyxml2::XMLDocument xml_doc;

		tinyxml2::XMLError result = xml_doc.LoadFile(load_context.render_passes_filename);
		if (result != tinyxml2::XML_SUCCESS)
		{
			core::LogError("Error loading <%s> render passes declaration, file doesn't exist", load_context.render_passes_filename);
			return false;
		}

		tinyxml2::XMLNode* root = xml_doc.FirstChildElement("Root");
		if (root == nullptr)
		{
			core::LogError("Error loading <%s> render passes declaration, Root node doesn't exist", load_context.render_passes_filename);
			return false;
		}

		//Set the xml doc to the load context
		load_context.xml_doc = &xml_doc;

		//Load global resources
		tinyxml2::XMLElement* resource = root->FirstChildElement("Global");
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
				core::LogError("Error loading <%s> render passes declaration, global element <%s> not supported", load_context.render_passes_filename, resource->Name());
			}

			resource = resource->NextSiblingElement();
		}


		return true;
	}


	System * CreateRenderPassSystem()
	{
		System* system = new System();
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

		return system->Load(load_context);
	}

	bool RegisterResourceFactory(System * system, const char * resource_type, std::unique_ptr<FactoryInterface<Resource>> resource_factory)
	{
		if (system->m_resource_factories_map.find(resource_type) != system->m_resource_factories_map.end())
		{
			core::LogWarning("Resource <%s> has been already added, discarting new resource type", resource_type);
			return false;
		}
		system->m_resource_factories_map[resource_type] = std::move(resource_factory);
		return true;
	}

	bool RegisterPassFactory(System * system, const char * pass_type, std::unique_ptr<FactoryInterface<Pass>> pass_factory)
	{
		if (system->m_resource_factories_map.find(pass_type) != system->m_resource_factories_map.end())
		{
			core::LogWarning("Pass <%s> has been already added, discarting new pass type", pass_type);
			return false;
		}

		system->m_pass_factories_map[pass_type] = std::move(pass_factory);
		return true;
	}

}