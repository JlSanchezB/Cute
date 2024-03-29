#include "render_pass.h"
#include <ext/tinyxml2/tinyxml2.h>
#include <render/render_helper.h>
#include <display/display.h>
#include "render_system.h"
#include <render/render_resource.h>
#include <string>
#include <core/profile.h>

namespace
{
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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

	template<>
	struct ConversionTable<display::TranstitionState>
	{
		constexpr static std::pair<const char*, display::TranstitionState> table[] =
		{
			{"Common", display::TranstitionState::Common},
			{"VertexAndConstantBuffer", display::TranstitionState::VertexAndConstantBuffer},
			{"RenderTarget", display::TranstitionState::RenderTarget},
			{"UnorderedAccess", display::TranstitionState::UnorderedAccess},
			{"PixelShaderResource", display::TranstitionState::PixelShaderResource},
			{"NonPixelShaderResource", display::TranstitionState::NonPixelShaderResource},
			{"AllShaderResource", display::TranstitionState::AllShaderResource},
			{"Depth", display::TranstitionState::Depth},
			{"DepthRead", display::TranstitionState::DepthRead},
			{"IndirectArgument", display::TranstitionState::IndirectArgument}
		};
	};

	template<>
	struct ConversionTable<display::Format>
	{
		constexpr static std::pair<const char*, display::Format> table[] =
		{
			{"UNKNOWN", display::Format::UNKNOWN},
			{"R32G32_FLOAT", display::Format::R32G32_FLOAT},
			{"R32G32B32_FLOAT", display::Format::R32G32B32_FLOAT},
			{"R32G32B32A32_FLOAT", display::Format::R32G32B32A32_FLOAT},
			{"R8G8B8A8_UNORM", display::Format::R8G8B8A8_UNORM},
			{"R8G8B8A8_UNORM_SRGB", display::Format::R8G8B8A8_UNORM_SRGB},
			{"R32_UINT", display::Format::R32_UINT},
			{"R16_UINT", display::Format::R16_UINT},
			{"D32_FLOAT", display::Format::D32_FLOAT},
			{"R32_FLOAT", display::Format::R32_FLOAT},
			{"R16G16B16A16_FLOAT", display::Format::R16G16B16A16_FLOAT}
		};
	};

	template<>
	struct ConversionTable<display::ClearType>
	{
		constexpr static std::pair<const char*, display::ClearType> table[] =
		{
			{"Depth", display::ClearType::Depth},
			{"Stencil", display::ClearType::Stencil},
			{"DepthStencil", display::ClearType::DepthStencil}
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
			if (item)
			{
				item->Destroy(device);
			}
		}
	}
	void ContextPass::Load(LoadContext & load_context)
	{
		//This pass is a list of passes that get added into a context
		m_command_list_handle = display::CreateCommandList(load_context.device, load_context.pass_name);
		m_name = PassName(load_context.pass_name);

		auto xml_element = load_context.current_xml_element->FirstChildElement();

		while (xml_element)
		{
			if (strcmp(xml_element->Name(), "Dependencies") == 0)
			{
				//Read all pre conditions
				auto dependencies_xml_element = xml_element->FirstChildElement();
				while (dependencies_xml_element)
				{
					if (strcmp(dependencies_xml_element->Name(), "Resource") == 0)
					{
						const char* string_value;
						ResourceName resource_name;
						if (dependencies_xml_element->QueryStringAttribute("name", &string_value) == tinyxml2::XML_SUCCESS)
						{
							resource_name = ResourceName(string_value);
						}
						else
						{
							AddError(load_context, "Error reading a resource state attribute <%s> in node <%s>", "name", load_context.name);
						}

						if (dependencies_xml_element->QueryStringAttribute("pre_condition_state", &string_value) == tinyxml2::XML_SUCCESS)
						{
							//Add a pre condition state
							m_pre_resource_conditions.emplace_back(resource_name, ResourceState(string_value));
						}

						if (dependencies_xml_element->QueryStringAttribute("post_update_state", &string_value) == tinyxml2::XML_SUCCESS)
						{
							//Add a post update state
							m_post_resource_updates.emplace_back(resource_name, ResourceState(string_value));
						}

						display::TranstitionState access;
						if (QueryTableAttribute(load_context, dependencies_xml_element, "access", access, AttributeType::Optional))
						{
							m_resource_barriers.emplace_back(resource_name, access);
						}
					}
					else
					{
						PoolResourceType type;
						bool valid = false;
						if (strcmp(dependencies_xml_element->Name(), "RenderTarget") == 0)
						{
							valid = true;
							type = PoolResourceType::RenderTarget;
						}
						else if (strcmp(dependencies_xml_element->Name(), "DepthBuffer") == 0)
						{
							valid = true;
							type = PoolResourceType::DepthBuffer;
						}
						else if (strcmp(dependencies_xml_element->Name(), "Texture2D") == 0)
						{
							valid = true;
							type = PoolResourceType::Texture2D;
						}
						else if (strcmp(dependencies_xml_element->Name(), "Buffer") == 0)
						{
							valid = true;
							type = PoolResourceType::Buffer;
						}

						if (valid)
						{
							const char* string_value;
							const char* pre_condition_state;
							const char* post_condition_state;
							ResourceName resource_name;
							ResourceState pre_condition;
							ResourceState post_condition;
							if (dependencies_xml_element->QueryStringAttribute("name", &string_value) == tinyxml2::XML_SUCCESS)
							{
								resource_name = ResourceName(string_value);
							}
							else
							{
								AddError(load_context, "Error reading a render target attribute <%s> in node <%s>", "name", load_context.name);
							}

							if (dependencies_xml_element->QueryStringAttribute("pre_condition_state", &pre_condition_state) == tinyxml2::XML_SUCCESS)
							{
								pre_condition = ResourceState(pre_condition_state);
								if (pre_condition != "Alloc"_sh32)
								{
									//Add a pre condition state
									m_pre_resource_conditions.emplace_back(resource_name, ResourceState(pre_condition_state));
								}
							}

							if (dependencies_xml_element->QueryStringAttribute("post_update_state", &post_condition_state) == tinyxml2::XML_SUCCESS)
							{
								post_condition = ResourceState(post_condition_state);
								if (post_condition != "Free"_sh32)
								{
									//Add a post update state
									m_post_resource_updates.emplace_back(resource_name, ResourceState(post_condition_state));
								}
							}

							display::TranstitionState access;
							if (QueryTableAttribute(load_context, dependencies_xml_element, "access", access, AttributeType::Optional))
							{
								m_resource_barriers.emplace_back(resource_name, access);
							}
							float width_factor = 1.f;
							dependencies_xml_element->QueryFloatAttribute("width_factor", &width_factor);
							float height_factor = 1.f;
							dependencies_xml_element->QueryFloatAttribute("height_factor", &height_factor);

							uint32_t tile_size_width = 1;
							dependencies_xml_element->QueryUnsignedAttribute("tile_size_width", &tile_size_width);

							uint32_t tile_size_height = 1;
							dependencies_xml_element->QueryUnsignedAttribute("tile_size_height", &tile_size_height);

							uint32_t width = 0;
							dependencies_xml_element->QueryUnsignedAttribute("width", &width);

							uint32_t height = 0;
							dependencies_xml_element->QueryUnsignedAttribute("height", &height);

							uint32_t size = 0;
							dependencies_xml_element->QueryUnsignedAttribute("size", &size);

							bool clear = false;
							dependencies_xml_element->QueryBoolAttribute("clear", &clear);

							bool not_alias = false;
							dependencies_xml_element->QueryBoolAttribute("not_alias", &not_alias);

							display::Format format = display::Format::UNKNOWN;
							QueryTableAttribute(load_context, dependencies_xml_element, "format", format, AttributeType::Optional);

							float default_depth = 1.f;
							dependencies_xml_element->QueryFloatAttribute("default_depth", &default_depth);

							uint32_t default_stencil = 0;
							dependencies_xml_element->QueryUnsignedAttribute("default_stencil", &default_stencil);

							bool is_uav = false;
							dependencies_xml_element->QueryBoolAttribute("uav", &is_uav);

							//Add resource dependency, so the pass will request the resource to the pool
							m_resource_pool_dependencies.emplace_back(resource_name, type, pre_condition == "Alloc"_sh32, post_condition == "Free"_sh32, width, height, size, width_factor, height_factor, static_cast<uint16_t>(tile_size_width), static_cast<uint16_t>(tile_size_height),format, default_depth, static_cast<uint8_t>(default_stencil), clear, not_alias, is_uav);


							//Needs to access the resource, it will be empty at the moment, as it is going to get assigned during the pass
							load_context.AddPoolResource(resource_name);
						}
					}

					dependencies_xml_element = dependencies_xml_element->NextSiblingElement();
				}
			}
			
			else if (strcmp(xml_element->Name(), "Commands") == 0)
			{
				//Read all commands associated to this pass
				auto commands_xml_element = xml_element->FirstChildElement();
				while (commands_xml_element)
				{
					load_context.current_xml_element = commands_xml_element;
					load_context.name = commands_xml_element->Name();

					//Load each of the passes and add them in the vector
					m_passes.emplace_back(load_context.render_system->LoadPass(load_context));

					commands_xml_element = commands_xml_element->NextSiblingElement();
				}
			}
			
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
		//This call is invalid, calls to render for a ContextPass must be done with the resource_transitions
		assert(true);
	}
	void ContextPass::Execute(RenderContext & render_context) const
	{
		for (auto& item : m_passes)
		{
			item->Execute(render_context);
		}

		display::ExecuteCommandList(render_context.GetDevice(), m_command_list_handle);
	}

	void ContextPass::RootContextRender(RenderContext& render_context, const std::vector<display::ResourceBarrier>& resource_barriers) const
	{
		//Open context
		render_context.SetContext(display::OpenCommandList(render_context.GetDevice(), m_command_list_handle));
		{
			PROFILE_SCOPE_GPU_ARG(render_context.GetContext(), "Render", kRenderProfileColour, "Render Pass <%s>", m_name.GetValue());
			if (resource_barriers.size() > 0)
			{
				render_context.GetContext()->AddResourceBarriers(resource_barriers);
			}

			for (auto& item : m_passes)
			{
				item->Render(render_context);
			}
		}
		//Close context
		display::CloseCommandList(render_context.GetDevice(), render_context.GetContext());
	}

	void SetRenderTargetPass::Load(LoadContext & load_context)
	{
		m_num_render_targets = 0;

		auto xml_element = load_context.current_xml_element->FirstChildElement();

		while (xml_element)
		{
			if (strcmp(xml_element->Name(), "RenderTarget") == 0)
			{
				load_context.current_xml_element = xml_element;
				m_render_target[m_num_render_targets].UpdateName(load_context.GetResourceReference(load_context));

				m_num_render_targets++;
				
				if (m_num_render_targets == display::kMaxNumRenderTargets)
				{
					AddError(load_context, "Max number of render target reached loading the pass SetRenderTargets");
					return;
				}
			}
			else if (strcmp(xml_element->Name(), "DepthBuffer") == 0)
			{
				load_context.current_xml_element = xml_element;
				m_depth_buffer.UpdateName(load_context.GetResourceReference(load_context));
			}

			xml_element = xml_element->NextSiblingElement();
		}
	}

	void SetRenderTargetPass::Render(RenderContext & render_context) const
	{
		//Get render target
		std::array<display::AsRenderTarget, display::kMaxNumRenderTargets> render_targets;
		for (uint8_t i = 0; i < m_num_render_targets; ++i)
		{
			RenderTargetResource* render_target = m_render_target[i].Get(render_context);
			if (render_target)
			{
				render_targets[i] = display::AsRenderTarget(render_target->GetHandle());
			}
		}
		DepthBufferResource* depth_buffer = m_depth_buffer.Get(render_context);

		render_context.GetContext()->SetRenderTargets(m_num_render_targets, render_targets.data(), (depth_buffer) ? display::AsDepthBuffer(depth_buffer->GetHandle()) : display::AsDepthBuffer());
		
		//Set Viewport and Scissors (just using the texture size, we need to add an interface for it)
		uint32_t width, height;
		display::GetTexture2DDimensions(render_context.GetDevice(), render_targets[0], width, height);
		
		//Set viewport
		render_context.GetContext()->SetViewport(display::Viewport(static_cast<float>(width), static_cast<float>(height)));

		//Set Scissor Rect
		render_context.GetContext()->SetScissorRect(display::Rect(0, 0, width, height));
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
		m_render_target.UpdateName(load_context.GetResourceReference(load_context));
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
	void ClearDepthStencilPass::Load(LoadContext& load_context)
	{
		float depth_value_read = 0.f;
		uint8_t stencil_value_read = 0;
		if (QueryAttribute(load_context, load_context.current_xml_element, "depth", depth_value_read, AttributeType::Optional))
		{
			depth_value = depth_value_read;
		}
		if (QueryAttribute(load_context, load_context.current_xml_element, "stencil", stencil_value_read, AttributeType::Optional))
		{
			stencil_value = stencil_value_read;
		}
		clear_type = display::ClearType::Depth;
		QueryTableAttribute(load_context, load_context.current_xml_element, "type", clear_type, AttributeType::Optional);

		m_depth_stencil_buffer.UpdateName(load_context.GetResourceReference(load_context));
	}
	void ClearDepthStencilPass::Render(RenderContext& render_context) const
	{
		//Get render target
		DepthBufferResource* depth_stencil_resource = m_depth_stencil_buffer.Get(render_context);
		if (depth_stencil_resource)
		{
			render_context.GetContext()->ClearDepthStencil(depth_stencil_resource->GetHandle(), clear_type, depth_value, stencil_value);
		}
	}
	void SetRootSignaturePass::Load(LoadContext & load_context)
	{
		QueryTableAttribute(load_context, load_context.current_xml_element, "pipe", m_pipe, AttributeType::Optional);
		m_rootsignature.UpdateName(load_context.GetResourceReference(load_context));
	}
	void SetRootSignaturePass::Render(RenderContext & render_context) const
	{
		RootSignatureResource* root_signature = m_rootsignature.Get(render_context);
		if (root_signature)
		{
			render_context.GetContext()->SetRootSignature(m_pipe, root_signature->GetHandle());
		}
	}
	void SetRootConstantBufferPass::Load(LoadContext & load_context)
	{
		QueryTableAttribute(load_context, load_context.current_xml_element, "pipe", m_pipe, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "root_param", m_root_parameter, AttributeType::NonOptional);
		m_constant_buffer.UpdateName(load_context.GetResourceReference(load_context));
	}
	void SetRootConstantBufferPass::Render(RenderContext & render_context) const
	{
		BufferResource* constant_buffer = m_constant_buffer.Get(render_context);
		if (constant_buffer)
		{
			render_context.GetContext()->SetConstantBuffer(m_pipe, m_root_parameter, constant_buffer->GetHandle());
		}
	}
	void SetRootUnorderedAccessBufferPass::Load(LoadContext& load_context)
	{
		QueryTableAttribute(load_context, load_context.current_xml_element, "pipe", m_pipe, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "root_param", m_root_parameter, AttributeType::NonOptional);
		m_unordered_access_buffer.UpdateName(load_context.GetResourceReference(load_context));
	}
	void SetRootUnorderedAccessBufferPass::Render(RenderContext& render_context) const
	{
		BufferResource* unordered_access_buffer = m_unordered_access_buffer.Get(render_context);
		if (unordered_access_buffer)
		{
			render_context.GetContext()->SetUnorderedAccessBuffer(m_pipe, m_root_parameter, unordered_access_buffer->GetHandle());
		}
	}
	void SetRootShaderResourcePass::Load(LoadContext& load_context)
	{
		QueryTableAttribute(load_context, load_context.current_xml_element, "pipe", m_pipe, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "root_param", m_root_parameter, AttributeType::NonOptional);
		m_shader_resource.UpdateName(load_context.GetResourceReference(load_context));
	}
	void SetRootShaderResourcePass::Render(RenderContext& render_context) const
	{
		BufferResource* shader_resource = m_shader_resource.Get(render_context);
		if (shader_resource)
		{
			render_context.GetContext()->SetShaderResource(m_pipe, m_root_parameter, shader_resource->GetHandle());
		}
	}
	void SetPipelineStatePass::Load(LoadContext & load_context)
	{
		m_pipeline_state.UpdateName(load_context.GetResourceReference(load_context));
	}
	void SetPipelineStatePass::Render(RenderContext & render_context) const
	{
		GraphicsPipelineStateResource* pipeline_state = m_pipeline_state.Get(render_context);
		if (pipeline_state)
		{
			render_context.GetContext()->SetPipelineState(pipeline_state->GetHandle());
		}
		else
		{
			core::LogError("Pipeline <%s> doesn't exist", m_pipeline_state.GetResourceName().GetValue());
		}
	}
	void SetComputePipelineStatePass::Load(LoadContext& load_context)
	{
		m_pipeline_state.UpdateName(load_context.GetResourceReference(load_context));
	}
	void SetComputePipelineStatePass::Render(RenderContext& render_context) const
	{
		ComputePipelineStateResource* pipeline_state = m_pipeline_state.Get(render_context);
		if (pipeline_state)
		{
			render_context.GetContext()->SetPipelineState(pipeline_state->GetHandle());
		}
		else
		{
			core::LogError("Pipeline <%s> doesn't exist", m_pipeline_state.GetResourceName().GetValue());
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
			m_descriptor_table.UpdateName(load_context.GetResourceReference(load_context));
			return;
		}

		auto xml_element_descriptor = load_context.current_xml_element->FirstChildElement("Descriptor");
		if (xml_element_descriptor)
		{
			//It is a descriptor that has to be created during init pass
			m_descriptor_table.UpdateName(ResourceName((std::string("DescriptorTable_") + std::to_string(m_resource_id_count++)).c_str()));

			while (xml_element_descriptor)
			{
				display::DescriptorTableParameterType parameter_type = display::DescriptorTableParameterType::ShaderResource;
				const char* string_value;
				if (xml_element_descriptor->QueryStringAttribute("as", &string_value) == tinyxml2::XML_SUCCESS)
				{
					if (strcmp("UnorderedAccess", string_value) == 0)
					{
						parameter_type = display::DescriptorTableParameterType::UnorderedAccessBuffer;
					}
				}

				m_descriptor_table_names.emplace_back(xml_element_descriptor->GetText(), parameter_type);

				//It is a descriptor list, names need to be solve during render
				xml_element_descriptor = xml_element_descriptor->NextSiblingElement();
			}
			//The descriptor table will be created during init pass

			return;
		}
		
		AddError(load_context, "SetDescriptorTablePass uknown definition");
	}
	bool SetDescriptorTablePass::FillDescriptorTableDesc(RenderContext& render_context, display::DescriptorTableDesc& descriptor_table_desc) const
	{
		bool descriptor_full_inited = true;
		for (auto& descriptor : m_descriptor_table_names)
		{
			bool pass_resource;
			Resource* resource = render_context.GetResource(ResourceName(descriptor.first.c_str()), pass_resource);

			if (resource)
			{
				auto display_handle = resource->GetDisplayHandle();

				std::visit(
					overloaded
					{
						[&](display::WeakBufferHandle handle)
						{
							if (descriptor.second == display::DescriptorTableParameterType::UnorderedAccessBuffer)
								descriptor_table_desc.AddDescriptor(display::AsUAVBuffer(handle));
							else
								descriptor_table_desc.AddDescriptor(handle);
						},
						[&](display::WeakTexture2DHandle handle)
						{
							if (descriptor.second == display::DescriptorTableParameterType::UnorderedAccessBuffer)
								descriptor_table_desc.AddDescriptor(display::AsUAVTexture2D(handle));
							else
								descriptor_table_desc.AddDescriptor(handle);
						},
						[&](std::monostate handle)
						{
						}
					}, display_handle);
			}
			else
			{
				descriptor_full_inited = false;
				//Descriptor has some resources that can not be built until the rendering, for example a pool resource
				descriptor_table_desc.AddDescriptor(display::DescriptorTableDesc::NullDescriptor());
			}
		}

		return descriptor_full_inited;
	}
	void SetDescriptorTablePass::InitPass(RenderContext & render_context, display::Device * device, ErrorContext& errors)
	{
		//Create a descriptor table resource and add it to render context
		display::DescriptorTableDesc descriptor_table_desc;
		descriptor_table_desc.access = display::Access::Dynamic;

		m_update_each_frame = !FillDescriptorTableDesc(render_context, descriptor_table_desc);

		display::DescriptorTableHandle descriptor_table_handle = display::CreateDescriptorTable(device, descriptor_table_desc);

		if (!descriptor_table_handle.IsValid())
		{
			AddError(errors, "Error creation descritpor table, display errors:", display::GetLastErrorMessage(device));
		}

		render_context.AddPassResource(m_descriptor_table.GetResourceName(), CreateResourceFromHandle<render::DescriptorTableResource>(descriptor_table_handle));
	}
	void SetDescriptorTablePass::Render(RenderContext & render_context) const
	{
		DescriptorTableResource* descriptor_table = m_descriptor_table.Get(render_context);
		if (descriptor_table)
		{
			if (m_update_each_frame)
			{
				//Capture all descriptors
				display::DescriptorTableDesc descriptor_table_desc;
				FillDescriptorTableDesc(render_context, descriptor_table_desc);

				//update descriptors
				display::UpdateDescriptorTable(render_context.GetDevice(), descriptor_table->GetHandle(), descriptor_table_desc.descriptors.data(), descriptor_table_desc.num_descriptors);
			}

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

			display::BufferDesc vertex_buffer_desc = display::BufferDesc::CreateVertexBuffer(display::Access::Static, sizeof(vertex_data), sizeof(VertexData), vertex_data);
			display::BufferHandle vertex_buffer = display::CreateBuffer(load_context.device, vertex_buffer_desc, "fullscreen_quad");

			//Add the resource
			load_context.AddResource("DrawFullScreenQuadPassVertexBuffer"_sh32, CreateResourceFromHandle<BufferResource>(vertex_buffer));
		}
	}
	void DrawFullScreenQuadPass::Render(RenderContext & render_context) const
	{
		bool pass_resource;
		BufferResource* vertex_buffer = render_context.GetResource<BufferResource>("DrawFullScreenQuadPassVertexBuffer"_sh32, pass_resource);
		if (vertex_buffer)
		{
			render_context.GetContext()->SetVertexBuffers(0, 1, &vertex_buffer->GetHandle());

			display::DrawDesc draw_desc;
			draw_desc.vertex_count = 3;
			render_context.GetContext()->Draw(draw_desc);
		}
	}

	void DispatchViewComputePass::Load(LoadContext& load_context)
	{
		QueryAttribute(load_context, load_context.current_xml_element, "tile_width", m_tile_width, AttributeType::NonOptional);
		QueryAttribute(load_context, load_context.current_xml_element, "tile_height", m_tile_height, AttributeType::NonOptional);
		QueryAttribute(load_context, load_context.current_xml_element, "scale_down", m_scale_down, AttributeType::Optional);
	}
	void DispatchViewComputePass::Render(RenderContext& render_context) const
	{
		//Knowing the view size and the tile size, calculate how many groups needs to dispatch
		display::ExecuteComputeDesc desc;

		desc.group_count_x = display::ExecuteComputeDesc::CalculateGroupCount(render_context.GetPassInfo().width, m_tile_width * m_scale_down);
		desc.group_count_y = display::ExecuteComputeDesc::CalculateGroupCount(render_context.GetPassInfo().height, m_tile_height * m_scale_down);
		desc.group_count_z = 1;

		render_context.GetContext()->ExecuteCompute(desc);
	}

	void DispatchComputePass::Load(LoadContext& load_context)
	{
		QueryAttribute(load_context, load_context.current_xml_element, "group_count_x", m_group_count_x, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "group_count_y", m_group_count_y, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "group_count_z", m_group_count_z, AttributeType::Optional);
	}
	void DispatchComputePass::Render(RenderContext& render_context) const
	{
		//Knowing the view size and the tile size, calculate how many groups needs to dispatch
		display::ExecuteComputeDesc desc;

		desc.group_count_x = m_group_count_x;
		desc.group_count_y = m_group_count_y;
		desc.group_count_z = m_group_count_z;

		render_context.GetContext()->ExecuteCompute(desc);
	}

	void DispatchComputeFilterPass::Load(LoadContext& load_context)
	{
		QueryAttribute(load_context, load_context.current_xml_element, "tile_size_x", m_tile_size_x, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "tile_size_y", m_tile_size_y, AttributeType::Optional);
		
		m_texture.UpdateName(load_context.GetResourceReference(load_context));
	}
	void DispatchComputeFilterPass::Render(RenderContext& render_context) const
	{
		//Get the size of the filter texture
		TextureResource* texture = m_texture.Get(render_context);
		if (texture)
		{
			uint32_t width, height;
			display::GetTexture2DDimensions(render_context.GetDevice(), texture->GetHandle(), width, height);

			display::ExecuteComputeDesc desc;
			desc.group_count_x = ((width - 1) / m_tile_size_x) + 1;
			desc.group_count_y = ((height - 1) / m_tile_size_x) + 1;
			desc.group_count_z = 1;

			render_context.GetContext()->ExecuteCompute(desc);
		}
	}

	void DrawRenderItemsPass::Load(LoadContext & load_context)
	{
		const char* value;
		if (load_context.current_xml_element->QueryStringAttribute("priority", &value) == tinyxml2::XML_SUCCESS)
		{
			m_priority = GetRenderItemPriority(load_context.render_system, PriorityName(value));
		}
		else
		{
			AddError(load_context, "Attribute priority expected inside DrawRenderItems pass");
		}
	}
	void DrawRenderItemsPass::Render(RenderContext & render_context) const
	{
		auto& render_context_internal = *reinterpret_cast<const RenderContextInternal*>(&render_context);

		assert(render_context_internal.m_point_of_view);

		auto& context = render_context_internal.m_display_context;
		auto& render_items = render_context_internal.m_point_of_view->m_sorted_render_items;
		const size_t begin_render_item = render_context_internal.m_point_of_view->m_sorted_render_items.m_priority_table[m_priority].first;
		const size_t end_render_item = render_context_internal.m_point_of_view->m_sorted_render_items.m_priority_table[m_priority].second;
		if (begin_render_item != -1)
		{
			//It has something to render
			for (size_t render_item_index = begin_render_item; render_item_index <= end_render_item; ++render_item_index)
			{
				auto& render_item = render_items.m_sorted_render_items[render_item_index];

				auto& command_buffer = render_context_internal.m_point_of_view->m_command_buffer.AccessThreadData(render_item.command_worker);

				//Execute commands for this render item
				command_buffer.Execute(*context, render_item.command_offset);
			}
		}

	}
}