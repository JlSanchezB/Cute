#include <render/render_resource.h>
#include <ext/tinyxml2/tinyxml2.h>
#include <render/render_helper.h>
#include <display/display.h>

#include <fstream>

namespace
{
	std::vector<char> ReadFileToBuffer(const char* file_name)
	{
		std::ifstream file(file_name, std::ios::binary | std::ios::ate);
		if (file.good())
		{
			std::streamsize size = file.tellg();
			file.seekg(0, std::ios::beg);

			std::vector<char> buffer(size);
			file.read(buffer.data(), size);

			return buffer;
		}
		else
		{
			return std::vector<char>(0);
		}
	}

	//Conversion tables
	template<>
	struct ConversionTable<display::Access>
	{
		constexpr static std::pair<const char*, display::Access> table[] =
		{
			{"Static", display::Access::Static},
			{"Dynamic", display::Access::Dynamic}
		};
	};
	
	template<>
	struct ConversionTable<display::RootSignatureParameterType>
	{
		constexpr static std::pair<const char*, display::RootSignatureParameterType> table[] =
		{
			{"ConstantBuffer", display::RootSignatureParameterType::ConstantBuffer},
			{"Constants", display::RootSignatureParameterType::Constants},
			{"DescriptorTable", display::RootSignatureParameterType::DescriptorTable},
			{"ShaderResource", display::RootSignatureParameterType::ShaderResource},
			{"UnorderedAccessBuffer", display::RootSignatureParameterType::UnorderedAccessBuffer}
		};
	};

	template<>
	struct ConversionTable<display::DescriptorTableParameterType>
	{
		constexpr static std::pair<const char*, display::DescriptorTableParameterType> table[] =
		{
			{"ConstantBuffer", display::DescriptorTableParameterType::ConstantBuffer},
			{"UnorderedAccessBuffer", display::DescriptorTableParameterType::UnorderedAccessBuffer},
			{"ShaderResource", display::DescriptorTableParameterType::ShaderResource},
			{"Sampler", display::DescriptorTableParameterType::Sampler}
		};
	};

	template<>
	struct ConversionTable<display::ShaderVisibility>
	{
		constexpr static std::pair<const char*, display::ShaderVisibility> table[] =
		{
			{"All", display::ShaderVisibility::All},
			{"Domain", display::ShaderVisibility::Domain},
			{"Geometry", display::ShaderVisibility::Geometry},
			{"Hull", display::ShaderVisibility::Hull},
			{"Pixel", display::ShaderVisibility::Pixel},
			{"Vertex", display::ShaderVisibility::Vertex}
		};
	};

	template<>
	struct ConversionTable<display::Filter>
	{
		constexpr static std::pair<const char*, display::Filter> table[] =
		{
			{"Point", display::Filter::Point},
			{"Linear", display::Filter::Linear},
			{"Anisotropic", display::Filter::Anisotropic}
		};
	};

	template<>
	struct ConversionTable<display::TextureAddressMode>
	{
		constexpr static std::pair<const char*, display::TextureAddressMode> table[] =
		{
			{"Wrap", display::TextureAddressMode::Wrap},
			{"Mirror", display::TextureAddressMode::Mirror},
			{"Clamp", display::TextureAddressMode::Clamp}
		};
	};

	template<>
	struct ConversionTable<display::InputType>
	{
		constexpr static std::pair<const char*, display::InputType> table[] =
		{
			{"Instance", display::InputType::Instance},
			{"Vertex", display::InputType::Vertex}
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
			{"D32_FLOAT", display::Format::D32_FLOAT}
		};
	};

	template<>
	struct ConversionTable<display::CullMode>
	{
		constexpr static std::pair<const char*, display::CullMode> table[] =
		{
			{"Back", display::CullMode::Back},
			{"Front", display::CullMode::Front},
			{"None", display::CullMode::None}
		};
	};

	template<>
	struct ConversionTable<display::FillMode>
	{
		constexpr static std::pair<const char*, display::FillMode> table[] =
		{
			{"Solid", display::FillMode::Solid},
			{"Wireframe", display::FillMode::Wireframe}
		};
	};

	template<>
	struct ConversionTable<display::Blend>
	{
		constexpr static std::pair<const char*, display::Blend> table[] =
		{
			{"Solid", display::Blend::Zero},
			{"One", display::Blend::One},
			{"SrcAlpha", display::Blend::SrcAlpha},
			{"InvSrcAlpha", display::Blend::InvSrcAlpha}
		};
	};

	template<>
	struct ConversionTable<display::BlendOp>
	{
		constexpr static std::pair<const char*, display::BlendOp> table[] =
		{
			{"Add", display::BlendOp::Add},
			{"Substract", display::BlendOp::Substract}
		};
	};

	template<>
	struct ConversionTable<display::ComparationFunction>
	{
		constexpr static std::pair<const char*, display::ComparationFunction> table[] =
		{
			{"Never", display::ComparationFunction::Never},
			{"Less", display::ComparationFunction::Less},
			{"Equal", display::ComparationFunction::Equal},
			{"Less_Equal", display::ComparationFunction::Less_Equal},
			{"Greater", display::ComparationFunction::Greater},
			{"NotEqual", display::ComparationFunction::NotEqual},
			{"Greater_Equal", display::ComparationFunction::Greater_Equal},
			{"Always", display::ComparationFunction::Always}
		};
	};

	template<>
	struct ConversionTable<display::PrimitiveTopology>
	{
		constexpr static std::pair<const char*, display::PrimitiveTopology> table[] =
		{
			{"TriangleList", display::PrimitiveTopology::TriangleList}
		};
	};

	template<>
	struct ConversionTable<display::Topology>
	{
		constexpr static std::pair<const char*, display::Topology> table[] =
		{
			{"Triangle", display::Topology::Triangle}
		};
	};
}

namespace render
{
	void BoolResource::Load(LoadContext& load_context)
	{
		const char* value = load_context.current_xml_element->GetText();

		if (strcmp(value, "True") == 0)
		{
			m_value = true;
		}
		else if (strcmp(value, "False") == 0)
		{
			m_value = false;
		}
		else
		{
			AddError(load_context, "BoolResource <%s> doesn't have a 'True' or 'False' value", load_context.name);
		}
	}

	void TextureResource::Load(LoadContext& load_context)
	{
		const char* texture_filename = load_context.current_xml_element->GetText();

		//Load the texture file
		std::vector<char> texture_buffer;
		texture_buffer = ReadFileToBuffer(texture_filename);

		if (texture_buffer.size() > 0)
		{
			//Load the texture
			Init(display::CreateTextureResource(load_context.device, reinterpret_cast<void*>(&texture_buffer[0]), texture_buffer.size(), load_context.name));

			if (!GetHandle().IsValid())
			{
				AddError(load_context, "Error creating texture <%s>, display error <%>", texture_filename, display::GetLastErrorMessage(load_context.device));
			}
		}
		else
		{
			AddError(load_context, "Texture resource could not read file <%s>", texture_filename);
		}

	}

	void BufferResource::Load(LoadContext& load_context)
	{
		AddError(load_context, "Constant buffer declaraction not supported from render passes, only game");
	}

	void ConstantBufferResource::Load(LoadContext& load_context)
	{
		AddError(load_context, "Constant buffer declaraction not supported from render passes, only game");
	}

	void ConstantBuffer2Resource::Load(LoadContext& load_context)
	{
		AddError(load_context, "Constant buffer declaraction not supported from render passes, only game");
	}

	void UnorderedAccessBufferResource::Load(LoadContext& load_context)
	{
		AddError(load_context, "Unordered access buffer buffer declaraction not supported from render passes, only game");
	}

	void ShaderResourceResource::Load(LoadContext& load_context)
	{
		AddError(load_context, "Shader Resource declaraction not supported from render passes, only game");
	}

	void RootSignatureResource::Load(LoadContext& load_context)
	{
		display::RootSignatureDesc root_signature_desc;

		auto xml_element_root = load_context.current_xml_element->FirstChildElement();

		while (xml_element_root)
		{
			if (strcmp(xml_element_root->Name(), "RootParam") == 0)
			{
				//New root param
				auto& current_root_param = root_signature_desc.root_parameters[root_signature_desc.num_root_parameters++];

				if (root_signature_desc.num_root_parameters < display::kMaxNumRootParameters)
				{
					QueryTableAttribute(load_context, xml_element_root, "type", current_root_param.type, AttributeType::NonOptional);
					QueryTableAttribute(load_context, xml_element_root, "visibility", current_root_param.visibility, AttributeType::Optional);

					if (current_root_param.type == display::RootSignatureParameterType::DescriptorTable)
					{
						current_root_param.table.num_ranges = 0;

						//Read ranges
						auto xml_element_range = xml_element_root->FirstChildElement();

						while (xml_element_range)
						{
							//Add range
							if (strcmp(xml_element_range->Name(), "Range") == 0)
							{
								if (current_root_param.table.num_ranges == display::RootSignatureTable::kNumMaxRanges)
								{
									AddError(load_context, "Max number of range reach in root signature <%s>", load_context.name);
									return;
								}
								else
								{
									auto& range = current_root_param.table.range[current_root_param.table.num_ranges++];

									QueryTableAttribute(load_context, xml_element_range, "type", range.type, AttributeType::NonOptional);
									QueryAttribute(load_context, xml_element_range, "base_shader_register", range.base_shader_register, AttributeType::NonOptional);
									QueryAttribute(load_context, xml_element_range, "size", range.size, AttributeType::NonOptional);
								}
							}
							else
							{
								AddError(load_context, "Expected Range element inside root signature <%s>", load_context.name);
							}
							xml_element_range = xml_element_range->NextSiblingElement();
						}
					}
					else
					{
						//Read basic root constant
						QueryAttribute(load_context, xml_element_root, "shader_register", current_root_param.root_param.shader_register, AttributeType::NonOptional);
						QueryAttribute(load_context, xml_element_root, "num_constants", current_root_param.root_param.num_constants, AttributeType::Optional);
					}
				}
				else
				{
					AddError(load_context, "Max number of root parameters reach in root signature <%s>", load_context.name);
					return;
				}
			}
			else if (strcmp(xml_element_root->Name(), "StaticSample") == 0)
			{
				//New static sampler
				auto& current_static_sampler = root_signature_desc.static_samplers[root_signature_desc.num_static_samplers++];
				if (root_signature_desc.num_static_samplers <= display::kMaxNumStaticSamplers)
				{
					QueryAttribute(load_context, xml_element_root, "shader_register", current_static_sampler.shader_register, AttributeType::NonOptional);
					QueryTableAttribute(load_context, xml_element_root, "visibility", current_static_sampler.visibility, AttributeType::Optional);

					QueryTableAttribute(load_context, xml_element_root, "filter", current_static_sampler.filter, AttributeType::Optional);
					QueryTableAttribute(load_context, xml_element_root, "address_u", current_static_sampler.address_u, AttributeType::Optional);
					QueryTableAttribute(load_context, xml_element_root, "address_v", current_static_sampler.address_v, AttributeType::Optional);
					QueryTableAttribute(load_context, xml_element_root, "address_w", current_static_sampler.address_w, AttributeType::Optional);
				}
				else
				{
					AddError(load_context, "Max number of static sampler reach in root signature <%s>", load_context.name);
					return;
				}
			}
			else
			{
				AddError(load_context, "Invalid xml element found <%s> in root signature <%s>", xml_element_root->Name(), load_context.name);
			}

			xml_element_root = xml_element_root->NextSiblingElement();
		}

		//Create root signature
		Init(display::CreateRootSignature(load_context.device, root_signature_desc, load_context.name));

		if (!GetHandle().IsValid())
		{
			AddError(load_context, "Error creating root signature <%s>, display error <%>", load_context.name, display::GetLastErrorMessage(load_context.device));
		}
	}

	void RenderTargetResource::Load(LoadContext & load_context)
	{
		uint32_t width;
		uint32_t height;
		display::Format format;

		QueryTableAttribute(load_context, load_context.current_xml_element, "format", format, AttributeType::NonOptional);
		QueryAttribute(load_context, load_context.current_xml_element, "width", width, AttributeType::NonOptional);
		QueryAttribute(load_context, load_context.current_xml_element, "height", height, AttributeType::NonOptional);

		display::RenderTargetDesc render_target_desc;
		render_target_desc.width = width;
		render_target_desc.height = height;
		render_target_desc.format = format;

		//Create render target
		Init(display::CreateRenderTarget(load_context.device, render_target_desc, load_context.name));
		m_width = width;
		m_heigth = height;
	}

	void DepthBufferResource::Load(LoadContext & load_context)
	{
		//Depth buffers are dynamic, they are created during init pass when the resolution is known
	}

	void GraphicsPipelineStateResource::Load(LoadContext & load_context)
	{
		display::PipelineStateDesc pipeline_state_desc;

		std::vector<char> vertex_shader;
		std::vector<char> pixel_shader;
		
		QueryTableAttribute(load_context, load_context.current_xml_element, "primitive_topology", pipeline_state_desc.primitive_topology, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "depth_enable", pipeline_state_desc.depth_enable, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "depth_write", pipeline_state_desc.depth_write, AttributeType::Optional);
		QueryTableAttribute(load_context, load_context.current_xml_element, "depth_func", pipeline_state_desc.depth_func, AttributeType::Optional);
		QueryAttribute(load_context, load_context.current_xml_element, "stencil_enable", pipeline_state_desc.stencil_enable, AttributeType::Optional);
		QueryTableAttribute(load_context, load_context.current_xml_element, "depth_stencil_format", pipeline_state_desc.depth_stencil_format, AttributeType::Optional);

		auto xml_element_root = load_context.current_xml_element->FirstChildElement();

		while (xml_element_root)
		{
			if (CheckNodeName(xml_element_root, "RootSignature"))
			{
				//Root signature
				//Find resource
				RootSignatureResource* root_signature = GetResource<RootSignatureResource>(load_context.render_system, ResourceName(xml_element_root->GetText()));

				if (root_signature)
				{
					pipeline_state_desc.root_signature = root_signature->GetHandle();
				}
				else
				{
					AddError(load_context, "RootSignature <%s> doesn't exist in pipeline <%s>", xml_element_root->GetText(), load_context.name);
				}
			}
			else if (CheckNodeName(xml_element_root, "InputLayouts"))
			{
				auto xml_element_input = xml_element_root->FirstChildElement();

				while (xml_element_input)
				{
					if (CheckNodeName(xml_element_input, "Input"))
					{
						if (pipeline_state_desc.input_layout.num_elements <= display::kMaxNumInputLayoutElements)
						{
							auto& input_layout = pipeline_state_desc.input_layout.elements[pipeline_state_desc.input_layout.num_elements++];

							input_layout.semantic_name = xml_element_input->Attribute("semantic_name");
							if (input_layout.semantic_name == nullptr)
							{
								AddError(load_context, "Semantic name must be defined in pipeline state <%s>", load_context.name);
							}
							QueryAttribute(load_context, xml_element_input, "semantic_index", input_layout.semantic_index, AttributeType::NonOptional);
							QueryTableAttribute(load_context, xml_element_input, "format", input_layout.format, AttributeType::NonOptional);
							QueryAttribute(load_context, xml_element_input, "input_slot", input_layout.input_slot, AttributeType::NonOptional);
							QueryAttribute(load_context, xml_element_input, "aligned_offset", input_layout.aligned_offset, AttributeType::Optional);
							QueryTableAttribute(load_context, xml_element_input, "input_type", input_layout.input_type, AttributeType::Optional);
							QueryAttribute(load_context, xml_element_input, "instance_step_rate", input_layout.instance_step_rate, AttributeType::Optional);
						}
						else
						{
							AddError(load_context, "Max number of static sampler reach in pipeline state <%s>", load_context.name);
							return;
						}
					}
					else
					{
						AddError(load_context, "Only <Input> nodes are allow inside the input layout in pipeline state <%s>", load_context.name);
					}
					xml_element_input = xml_element_input->NextSiblingElement();
				}
			}
			else if (CheckNodeName(xml_element_root, "VertexShader"))
			{
				const char* entry_point = xml_element_root->Attribute("entry_point");
				const char* target = xml_element_root->Attribute("target");

				if (entry_point && target)
				{
					pipeline_state_desc.vertex_shader.file_name = xml_element_root->GetText();
					pipeline_state_desc.vertex_shader.target = target;
					pipeline_state_desc.vertex_shader.entry_point = entry_point;
					pipeline_state_desc.vertex_shader.name = load_context.name;
				}
				else
				{
					AddError(load_context, "Entry point or target missing in VertexShader in pipeline state <%s>", load_context.name);
					return;
				}
			}
			else if (CheckNodeName(xml_element_root, "PixelShader"))
			{
				const char* entry_point = xml_element_root->Attribute("entry_point");
				const char* target = xml_element_root->Attribute("target");

				if (entry_point && target)
				{
					pipeline_state_desc.pixel_shader.file_name = xml_element_root->GetText();
					pipeline_state_desc.pixel_shader.target = target;
					pipeline_state_desc.pixel_shader.entry_point = entry_point;
					pipeline_state_desc.pixel_shader.name = load_context.name;
				}
				else
				{
					AddError(load_context, "Entry point or target missing in PixelShader in pipeline state <%s>", load_context.name);
					return;
				}
			}
			else if (CheckNodeName(xml_element_root, "Rasterization"))
			{
				QueryTableAttribute(load_context, xml_element_root, "fill_mode", pipeline_state_desc.rasteritation_state.fill_mode, AttributeType::Optional);
				QueryTableAttribute(load_context, xml_element_root, "cull_mode", pipeline_state_desc.rasteritation_state.cull_mode, AttributeType::Optional);
				QueryAttribute(load_context, xml_element_root, "depth_bias", pipeline_state_desc.rasteritation_state.depth_bias, AttributeType::Optional);
				QueryAttribute(load_context, xml_element_root, "depth_bias_clamp", pipeline_state_desc.rasteritation_state.depth_bias_clamp, AttributeType::Optional);
				QueryAttribute(load_context, xml_element_root, "slope_depth_bias", pipeline_state_desc.rasteritation_state.slope_depth_bias, AttributeType::Optional);
				QueryAttribute(load_context, xml_element_root, "depth_clip_enable", pipeline_state_desc.rasteritation_state.depth_clip_enable, AttributeType::Optional);
			}
			else if (CheckNodeName(xml_element_root, "Blend"))
			{
				QueryAttribute(load_context, xml_element_root, "alpha_to_coverage_enable", pipeline_state_desc.blend_desc.alpha_to_coverage_enable, AttributeType::Optional);
				QueryAttribute(load_context, xml_element_root, "independent_blend_enable", pipeline_state_desc.blend_desc.independent_blend_enable, AttributeType::Optional);
			}
			else if (CheckNodeName(xml_element_root, "RenderTargets"))
			{
				//List all render target to extract blend and format information
				auto xml_element_render_target = xml_element_root->FirstChildElement();
				size_t render_target_count = 0;

				while (xml_element_render_target)
				{
					if (CheckNodeName(xml_element_render_target, "RenderTarget"))
					{
						auto& format = pipeline_state_desc.render_target_format[pipeline_state_desc.num_render_targets++];
						auto& blend = pipeline_state_desc.blend_desc.render_target_blend[render_target_count++];

						if (render_target_count < display::kMaxNumRenderTargets)
						{

							QueryTableAttribute(load_context, xml_element_render_target, "format", format, AttributeType::Optional);

							QueryAttribute(load_context, xml_element_render_target, "blend_enable", blend.blend_enable, AttributeType::Optional);
							QueryTableAttribute(load_context, xml_element_render_target, "src_blend", blend.src_blend, AttributeType::Optional);
							QueryTableAttribute(load_context, xml_element_render_target, "dest_blend", blend.dest_blend, AttributeType::Optional);
							QueryTableAttribute(load_context, xml_element_render_target, "blend_op", blend.blend_op, AttributeType::Optional);
							QueryTableAttribute(load_context, xml_element_render_target, "alpha_src_blend", blend.alpha_src_blend, AttributeType::Optional);
							QueryTableAttribute(load_context, xml_element_render_target, "alpha_dest_blend", blend.alpha_dest_blend, AttributeType::Optional);
							QueryTableAttribute(load_context, xml_element_render_target, "alpha_blend_op", blend.alpha_blend_op, AttributeType::Optional);
							QueryAttribute(load_context, xml_element_render_target, "write_mask", blend.write_mask, AttributeType::Optional);
						}
						else
						{
							AddError(load_context, "Max number of render targets reach in pipeline state <%s>", load_context.name);
							return;
						}
					}
					else
					{
						AddError(load_context, "Only <RenderTarget> nodes are allow inside the render targets in pipeline state <%s>", load_context.name);
					}
					xml_element_render_target = xml_element_render_target->NextSiblingElement();
				}
				pipeline_state_desc.num_render_targets = static_cast<uint8_t>(render_target_count);
			}
			else
			{
				AddError(load_context, "None <%s> invalid found in pipeline state <%s>", xml_element_root->Name(), load_context.name);
			}

			xml_element_root = xml_element_root->NextSiblingElement();
		}

		//Create pipeline state
		Init(display::CreatePipelineState(load_context.device, pipeline_state_desc, load_context.name));

		if (!GetHandle().IsValid())
		{
			AddError(load_context, "Error creating pipeline state <%s>, display error <%s>", load_context.name, display::GetLastErrorMessage(load_context.device));
		}
	}

	void ComputePipelineStateResource::Load(LoadContext & load_context)
	{
		display::ComputePipelineStateDesc pipeline_state_desc;

		std::vector<char> shader;

		auto xml_element_root = load_context.current_xml_element->FirstChildElement();

		while (xml_element_root)
		{
			if (CheckNodeName(xml_element_root, "RootSignature"))
			{
				//Root signature
				//Find resource
				RootSignatureResource* root_signature = GetResource<RootSignatureResource>(load_context.render_system, ResourceName(xml_element_root->GetText()));

				if (root_signature)
				{
					pipeline_state_desc.root_signature = root_signature->GetHandle();
				}
				else
				{
					AddError(load_context, "RootSignature <%s> doesn't exist in pipeline <%s>", xml_element_root->GetText(), load_context.name);
				}
			}
			else if (CheckNodeName(xml_element_root, "ComputeShader"))
			{
				const char* entry_point = xml_element_root->Attribute("entry_point");
				const char* target = xml_element_root->Attribute("target");

				if (entry_point && target)
				{
					pipeline_state_desc.compute_shader.file_name = xml_element_root->GetText();
					pipeline_state_desc.compute_shader.target = target;
					pipeline_state_desc.compute_shader.entry_point = entry_point;
					pipeline_state_desc.compute_shader.name = load_context.name;
				}
				else
				{
					AddError(load_context, "Entry point or target missing in ComputeShader in pipeline state <%s>", load_context.name);
					return;
				}
			}
			else
			{
				AddError(load_context, "None <%s> invalid found in pipeline state <%s>", xml_element_root->Name(), load_context.name);
			}

			xml_element_root = xml_element_root->NextSiblingElement();
		}

		//Create pipeline state
		Init(display::CreateComputePipelineState(load_context.device, pipeline_state_desc, load_context.name));

		if (!GetHandle().IsValid())
		{
			AddError(load_context, "Error creating pipeline state <%s>, display error <%s>", load_context.name, display::GetLastErrorMessage(load_context.device));
		}
	}

	void DescriptorTableResource::Load(LoadContext & load_context)
	{
		display::DescriptorTableDesc descriptor_table_desc;

		QueryTableAttribute(load_context, load_context.current_xml_element, "access", descriptor_table_desc.access, AttributeType::Optional);

		auto xml_element_root = load_context.current_xml_element->FirstChildElement();

		while (xml_element_root)
		{
			if (CheckNodeName(xml_element_root, "Descriptor"))
			{
				ResourceName resource_name(xml_element_root->GetText());
				
				if (descriptor_table_desc.num_descriptors == descriptor_table_desc.kNumMaxDescriptors)
				{
					AddError(load_context, "Number max of descriptor reach in descriptor table <%s>", load_context.name);
					return;
				}

				//Find resource
				if (auto resource = GetResource<ConstantBufferResource>(load_context.render_system, resource_name))
				{
					descriptor_table_desc.AddDescriptor(resource->GetHandle());
				}
				else if (auto resource = GetResource<TextureResource>(load_context.render_system, resource_name))
				{
					descriptor_table_desc.AddDescriptor(resource->GetHandle());
				}
				else
				{
					AddError(load_context, "Descriptor <%s> doesn't exist in descriptor table <%s>", xml_element_root->GetText(), load_context.name);
				}
			}
			else
			{
				AddError(load_context, "Only <Descriptor> nodes are supported inside a table descriptor <%s>", load_context.name);
			}
			xml_element_root = xml_element_root->NextSiblingElement();
		}

		//Create pipeline state
		Init(display::CreateDescriptorTable(load_context.device, descriptor_table_desc));

		if (!GetHandle().IsValid())
		{
			AddError(load_context, "Error creating descriptor table <%s>, display error <%>", load_context.name, display::GetLastErrorMessage(load_context.device));
		}
	}
}