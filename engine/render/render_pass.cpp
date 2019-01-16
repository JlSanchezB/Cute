#include "render_pass.h"
#include <ext/tinyxml2/tinyxml2.h>
#include "render_helper.h"
#include <display/display.h>
#include "render_system.h"
#include "render_resource.h"
#include <string>

namespace
{
	//Conversion tables
	template<>
	struct ConversionTable<display::Pipe>
	{
		constexpr static std::pair<const char*, display::Pipe> table[] =
		{
			{"Graphics", display::Pipe::Graphics},
			{"Compute", display::Pipe::Compute}
		};
	};
}

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
		m_command_list_handle = display::CreateCommandList(load_context.device, load_context.pass_name);

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
	void ContextPass::Render(RenderContext & render_context) const
	{
		//Open context
		render_context.SetContext(display::OpenCommandList(render_context.GetDevice(), m_command_list_handle));

		for (auto& item : m_passes)
		{
			item->Render(render_context);
		}

		//Close context
		display::CloseCommandList(render_context.GetDevice(), render_context.GetContext());
	}
	void ContextPass::Execute(RenderContext & render_context) const
	{
		for (auto& item : m_passes)
		{
			item->Execute(render_context);
		}

		display::ExecuteCommandList(render_context.GetDevice(), m_command_list_handle);
	}

	void SetRenderTargetPass::Load(LoadContext & load_context)
	{
		m_num_render_targets = 0;

		auto xml_element_render_target = load_context.current_xml_element->FirstChildElement("RenderTarget");

		while (xml_element_render_target)
		{
			load_context.current_xml_element = xml_element_render_target;
			m_render_target[m_num_render_targets].Set(load_context.GetResourceReference(load_context));

			m_num_render_targets++;
			xml_element_render_target = xml_element_render_target->NextSiblingElement();

			if (m_num_render_targets == display::kMaxNumRenderTargets)
			{
				AddError(load_context, "Max number of render target reached loading the pass SetRenderTargets");
				return;
			}
		}
	}
	void SetRenderTargetPass::Render(RenderContext & render_context) const
	{
		//Get render target
		std::array<display::WeakRenderTargetHandle, display::kMaxNumRenderTargets> render_targets;
		for (uint8_t i = 0; i < m_num_render_targets; ++i)
		{
			RenderTargetResource* render_target = m_render_target[i].Get(render_context);
			if (render_target)
			{
				render_targets[i] = render_target->GetHandle();
			}
		}
		RenderContext::PassInfo pass_info = render_context.GetPassInfo();

		render_context.GetContext()->SetRenderTargets(m_num_render_targets, render_targets.data(), display::WeakDepthBufferHandle());

		//Set Viewport and Scissors
		//Set viewport
		display::Viewport viewport(static_cast<float>(pass_info.width), static_cast<float>(pass_info.height));
		viewport.top_left_x = 0;
		viewport.top_left_y = 0;
		render_context.GetContext()->SetViewport(viewport);

		//Set Scissor Rect
		render_context.GetContext()->SetScissorRect(display::Rect(0, 0, pass_info.width, pass_info.height));
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
		m_render_target.Set(load_context.GetResourceReference(load_context));
	}
	void ClearRenderTargetPass::Render(RenderContext & render_context) const
	{
		//Get render target
		RenderTargetResource* render_target = m_render_target.Get(render_context);
		if (render_target)
		{
			render_context.GetContext()->ClearRenderTargetColour(render_target->GetHandle(), colour);
		}
	}
	void SetRootSignaturePass::Load(LoadContext & load_context)
	{
		QueryTableAttribute(load_context, load_context.current_xml_element, "pipe", m_pipe, AttributeType::Optional);
		m_rootsignature.Set(load_context.GetResourceReference(load_context));
	}
	void SetRootSignaturePass::Render(RenderContext & render_context) const
	{
		RootSignatureResource* root_signature = m_rootsignature.Get(render_context);
		if (root_signature)
		{
			render_context.GetContext()->SetRootSignature(m_pipe, root_signature->GetHandle());
		}
	}
	void SetPipelineStatePass::Load(LoadContext & load_context)
	{
		m_pipeline_state.Set(load_context.GetResourceReference(load_context));
	}
	void SetPipelineStatePass::Render(RenderContext & render_context) const
	{
		GraphicsPipelineStateResource* pipeline_state = m_pipeline_state.Get(render_context);
		if (pipeline_state)
		{
			render_context.GetContext()->SetPipelineState(pipeline_state->GetHandle());
		}
	}
	void SetDescriptorTablePass::Load(LoadContext & load_context)
	{
		QueryAttribute(load_context, load_context.current_xml_element, "root_param", m_root_parameter, AttributeType::NonOptional);
		QueryTableAttribute(load_context, load_context.current_xml_element, "pipe", m_pipe, AttributeType::Optional);

		auto xml_element_resource = load_context.current_xml_element->FirstChildElement("Resource");
		if (xml_element_resource)
		{
			//It is a resource
			m_descriptor_table.Set(load_context.GetResourceReference(load_context));
			return;
		}

		auto xml_element_descriptor = load_context.current_xml_element->FirstChildElement("Descriptor");
		if (xml_element_descriptor)
		{
			//It is a descriptor that has to be created during init pass
			m_descriptor_table.Set(ResourceName((std::string("DescriptorTable_") + std::to_string(rand())).c_str()));

			while (xml_element_descriptor)
			{
				m_descriptor_table_names.push_back(xml_element_descriptor->GetText());

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

		for (auto& descriptor : m_descriptor_table_names)
		{
			Resource* resource = render_context.GetRenderResource(ResourceName(descriptor.c_str()));

			if (resource)
			{
				if (resource->Type() == "ConstantBuffer"_sh32)
				{
					ConstantBufferResource* constant_buffer_resource = dynamic_cast<ConstantBufferResource*>(resource);
					descriptor_table_desc.AddDescriptor(constant_buffer_resource->GetHandle());
				}
				else if (resource->Type() == "Texture"_sh32)
				{
					TextureResource* texture_resource = dynamic_cast<TextureResource*>(resource);
					descriptor_table_desc.AddDescriptor(texture_resource->GetHandle());
				}
				else if (resource->Type() == "RenderTarget"_sh32)
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

		display::DescriptorTableHandle descriptor_table_handle = display::CreateDescriptorTable(device, descriptor_table_desc);

		if (descriptor_table_handle.IsValid())
		{
			//Create resource handle
			render_context.AddPassResource(m_descriptor_table.GetResourceName(), CreateResourceFromHandle<DescriptorTableResource>(descriptor_table_handle));
		}
		else
		{
			AddError(errors, "Error creation descritpor table, display errors:", display::GetLastErrorMessage(device));
		}

	}
	void SetDescriptorTablePass::Render(RenderContext & render_context) const
	{
		DescriptorTableResource* descriptor_table = m_descriptor_table.Get(render_context);
		if (descriptor_table)
		{
			render_context.GetContext()->SetDescriptorTable(m_pipe, m_root_parameter, descriptor_table->GetHandle());
		}
	}
	void DrawFullScreenQuadPass::Load(LoadContext & load_context)
	{
		//Create vertex buffer resource if it doesn't exist
		if (GetResource(load_context.render_system, "DrawFullScreenQuadPassVertexBuffer"_sh32) == nullptr)
		{
			struct VertexData
			{
				float position[4];
				float tex[2];
			};

			VertexData vertex_data[3] =
			{
				{{-1.f, 1.f, 1.f, 1.f},{0.f, 0.f}},
				{{3.f, 1.f, 1.f, 1.f},{2.f, 0.f}},
				{{-1.f, -3.f, 1.f, 1.f},{0.f, 2.f}}
			};

			display::VertexBufferDesc vertex_buffer_desc;
			vertex_buffer_desc.init_data = vertex_data;
			vertex_buffer_desc.size = sizeof(vertex_data);
			vertex_buffer_desc.stride = sizeof(VertexData);

			display::VertexBufferHandle vertex_buffer = display::CreateVertexBuffer(load_context.device, vertex_buffer_desc, "fullscreen_quad");

			//Add the resource
			load_context.AddResource("DrawFullScreenQuadPassVertexBuffer"_sh32, CreateResourceFromHandle<VertexBufferResource>(vertex_buffer));
		}
	}
	void DrawFullScreenQuadPass::Render(RenderContext & render_context) const
	{
		VertexBufferResource* vertex_buffer = render_context.GetResource<VertexBufferResource>("DrawFullScreenQuadPassVertexBuffer"_sh32);
		if (vertex_buffer)
		{
			render_context.GetContext()->SetVertexBuffers(0, 1, &vertex_buffer->GetHandle());

			display::DrawDesc draw_desc;
			draw_desc.vertex_count = 3;
			render_context.GetContext()->Draw(draw_desc);
		}
	}
}