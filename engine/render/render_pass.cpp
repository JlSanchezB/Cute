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
		m_descriptor_table_name = load_context.render_system->GetResourceReference(load_context);
	}
	void DrawFullScreenQuadPass::Load(LoadContext & load_context)
	{
	}
}