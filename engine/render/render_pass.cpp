#include "render_pass.h"
#include <ext/tinyxml2/tinyxml2.h>
#include "render_helper.h"
#include <display/display.h>
#include "render_system.h"
#include "render_resource.h"

namespace render
{
	void ContextPass::Destroy(display::Device * device)
	{
		if (m_command_list_handle.IsValid())
		{
			display::DestroyCommandList(device, m_command_list_handle);
		}

		for (auto& item : m_passes)
		{
			item->Destroy(device);
		}
	}
	void ContextPass::Load(LoadContext & load_context)
	{
		//This pass is a list of passes that get added into a context
		m_command_list_handle = display::CreateCommandList(load_context.device, load_context.name);

		auto xml_element = load_context.current_xml_element->FirstChildElement();

		while (xml_element)
		{
			load_context.current_xml_element = xml_element;
			load_context.name = xml_element->Name();

			//Load each of the passes and add them in the vector
			m_passes.emplace_back(load_context.render_system->LoadPass(load_context));
			
			xml_element = xml_element->NextSiblingElement();
		}
	}
	void ContextPass::InitPass(RenderContext & render_context, display::Device* device, ErrorContext& errors)
	{
		for (auto& item : m_passes)
		{
			item->InitPass(render_context, device, errors);
		}
	}
	void SetRenderTargetPass::Load(LoadContext & load_context)
	{
		auto xml_element_render_target = load_context.current_xml_element->FirstChildElement("RenderTarget");

		size_t i = 0;
		while (xml_element_render_target)
		{
			load_context.current_xml_element = xml_element_render_target;
			m_render_target_name[i] = load_context.render_system->GetResourceReference(load_context);

			i++;
			xml_element_render_target = xml_element_render_target->NextSiblingElement();

			if (i == display::kMaxNumRenderTargets)
			{
				AddError(load_context, "Max number of render target reached loading the pass SetRenderTargets");
				return;
			}
		}
		
	}
	void ClearRenderTargetPass::Load(LoadContext & load_context)
	{
		colour[0] = colour[1] = colour[2] = colour[3] = 0.f;
		const char* colour_text = load_context.current_xml_element->Attribute("colour");
		if (colour_text)
		{
			//Read four floats
			if (sscanf_s(colour_text, "%f,%f,%f,%f", &colour[0], &colour[1], &colour[2], &colour[3]) != 4)
			{
				AddError(load_context, "Colour can not be read from <%s>", colour_text);
			}
		}
	}
	void SetRootSignaturePass::Load(LoadContext & load_context)
	{
		m_rootsignature_name = load_context.render_system->GetResourceReference(load_context);
	}
	void SetPipelineStatePass::Load(LoadContext & load_context)
	{
		m_pipeline_state_name = load_context.render_system->GetResourceReference(load_context);
	}
	void SetDescriptorTablePass::Load(LoadContext & load_context)
	{
		auto xml_element_resource = load_context.current_xml_element->FirstChildElement("Resource");
		if (xml_element_resource)
		{
			//It is a resource
			m_descriptor_table_static_name = load_context.render_system->GetResourceReference(load_context);
			return;
		}

		auto xml_element_descriptor = load_context.current_xml_element->FirstChildElement("Descriptor");
		if (xml_element_descriptor)
		{
			//It is a descriptor that has to be created during init pass
			m_descriptor_table_static_name = "DescriptorTable" + rand();

			while (xml_element_descriptor)
			{
				m_descriptor_table.push_back(xml_element_descriptor->GetText());

				//It is a descriptor list, names need to be solve during render
				xml_element_descriptor = xml_element_descriptor->NextSiblingElement();
			}
			//The descriptor table will be created during init pass

			return;
		}
		
		AddError(load_context, "SetDescriptorTablePass uknown definition");
	}
	void SetDescriptorTablePass::InitPass(RenderContext & render_context, display::Device * device, ErrorContext& errors)
	{
		//Create a descriptor table resource and add it to render context
		display::DescriptorTableDesc descriptor_table_desc;
		descriptor_table_desc.access = display::Access::Dynamic;

		for (auto& descriptor : m_descriptor_table)
		{
			Resource* resource = render_context.GetRenderResource(descriptor.c_str());

			if (resource)
			{
				if (strcmp(resource->Type(), "ConstantBuffer") == 0)
				{
					ConstantBufferResource* constant_buffer_resource = dynamic_cast<ConstantBufferResource*>(resource);
					descriptor_table_desc.AddDescriptor(constant_buffer_resource->GetHandle());
				}
				else if (strcmp(resource->Type(), "Texture") == 0)
				{
					TextureResource* texture_resource = dynamic_cast<TextureResource*>(resource);
					descriptor_table_desc.AddDescriptor(texture_resource->GetHandle());
				}
				else if (strcmp(resource->Type(), "RenderTarget") == 0)
				{
					RenderTargetResource* render_target_resource = dynamic_cast<RenderTargetResource*>(resource);
					descriptor_table_desc.AddDescriptor(render_target_resource->GetHandle());
				}
			}
			else
			{
				AddError(errors, "Descriptor <%s> doesn't exist in the resource maps", descriptor.c_str());
			}
		}

		//Create descriptor table
		DescriptorTableResource* descriptor_table_resource = new DescriptorTableResource();

		display::DescriptorTableHandle descriptor_table_handle = display::CreateDescriptorTable(device, descriptor_table_desc);

		if (descriptor_table_handle.IsValid())
		{
			descriptor_table_resource->Init(descriptor_table_handle);
			std::unique_ptr<Resource> resource(dynamic_cast<Resource*>(descriptor_table_resource));
			render_context.AddRenderResource(m_descriptor_table_static_name.c_str(), resource);
		}
		else
		{
			AddError(errors, "Error creation descritpor table, display errors:", display::GetLastErrorMessage(device));
		}

	}
	void DrawFullScreenQuadPass::Load(LoadContext & load_context)
	{
	}
}