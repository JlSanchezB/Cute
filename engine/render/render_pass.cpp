#include "render_pass.h"
#include <ext/tinyxml2/tinyxml2.h>
#include "render_helper.h"
#include <display/display.h>
#include "render_system.h"

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
			m_descriptor_table.push_back(xml_element_descriptor->GetText());

			//It is a descriptor list, names need to be solve during render
			xml_element_descriptor = xml_element_descriptor->NextSiblingElement();
		}
	}
	void DrawFullScreenQuadPass::Load(LoadContext & load_context)
	{
	}
}