#include "manager.h"
#include <ext/tinyxml2/tinyxml2.h>

namespace render
{
	void Manager::LoadResource(render::LoadContext &load_context)
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

	bool Manager::Load(display::Device * device, const char * render_passes_declaration)
	{
		tinyxml2::XMLDocument xml_doc;

		tinyxml2::XMLError result = xml_doc.LoadFile(render_passes_declaration);
		if (result != tinyxml2::XML_SUCCESS)
		{
			core::LogError("Error loading <%s> render passes declaration, file doesn't exist", render_passes_declaration);
			return false;
		}

		tinyxml2::XMLNode* root = xml_doc.FirstChildElement("Root");
		if (root == nullptr)
		{
			core::LogError("Error loading <%s> render passes declaration, Root node doesn't exist", render_passes_declaration);
			return false;
		}

		LoadContext load_context;
		load_context.device = device;
		load_context.xml_doc = &xml_doc;
		load_context.render_passes_filename = render_passes_declaration;

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
}