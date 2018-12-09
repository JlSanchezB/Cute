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
	}
	void ClearRenderTargetPass::Load(LoadContext & load_context)
	{
	}
	void SetRootSignaturePass::Load(LoadContext & load_context)
	{
	}
	void SetPipelineStatePass::Load(LoadContext & load_context)
	{
	}
	void SetDescriptorTablePass::Load(LoadContext & load_context)
	{
	}
	void DrawFullScreenQuadPass::Load(LoadContext & load_context)
	{
	}
}