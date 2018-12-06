#include "render_resource.h"
#include <ext/tinyxml2/tinyxml2.h>
#include "render_helper.h"
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
			{"Constantbuffer", display::RootSignatureParameterType::ConstantBuffer},
			{"Constants", display::RootSignatureParameterType::Constants},
			{"DescriptorTable", display::RootSignatureParameterType::DescriptorTable},
			{"ShaderResource", display::RootSignatureParameterType::ShaderResource},
			{"UnorderedAccessBuffer", display::RootSignatureParameterType::UnorderAccessBuffer}
		};
	};

	template<>
	struct ConversionTable<display::DescriptorTableParameterType>
	{
		constexpr static std::pair<const char*, display::DescriptorTableParameterType> table[] =
		{
			{"Constantbuffer", display::DescriptorTableParameterType::ConstantBuffer},
			{"UnorderAccessBuffer", display::DescriptorTableParameterType::UnorderAccessBuffer},
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
			{"R16_UINT", display::Format::R16_UINT}
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
			GetHandle() = display::CreateTextureResource(load_context.device, reinterpret_cast<void*>(&texture_buffer[0]), texture_buffer.size(), load_context.name);

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

	void ConstantBufferResource::Load(LoadContext& load_context)
	{
		AddError(load_context, "Constant buffer declaraction not supported from render passes, only game");
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
						//Read ranges
						auto xml_element_range = xml_element_root->FirstChildElement();

						while (xml_element_range)
						{
							//Add range
							if (strcmp(xml_element_range->Name(), "RootParam") == 0)
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
		GetHandle() = display::CreateRootSignature(load_context.device, root_signature_desc, load_context.name);

		if (!GetHandle().IsValid())
		{
			AddError(load_context, "Error creating root signature <%s>, display error <%>", load_context.name, display::GetLastErrorMessage(load_context.device));
		}
	}

	void RenderTargetResource::Load(LoadContext & load_context)
	{
	}

	void DepthBufferResource::Load(LoadContext & load_context)
	{
	}

	void GraphicsPipelineStateResource::Load(LoadContext & load_context)
	{
		display::PipelineStateDesc pipeline_state_desc;

		std::vector<char> vertex_shader;
		std::vector<char> pixel_shader;
		
		auto xml_element_root = load_context.current_xml_element->FirstChildElement();

		if (CheckNodeName(xml_element_root, "RootSignature"))
		{
			//Root signature
			//Find resource
			RootSignatureResource* root_signature = GetResource<RootSignatureResource>(load_context.render_system, xml_element_root->GetText());

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
						QueryAttribute(load_context, xml_element_input, "aligned_offset", input_layout.aligned_offset, AttributeType::NonOptional);
						QueryTableAttribute(load_context, xml_element_input, "input_type", input_layout.input_type, AttributeType::NonOptional);
						QueryAttribute(load_context, xml_element_input, "instance_step_rate", input_layout.instance_step_rate, AttributeType::NonOptional);
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
		else if (CheckNodeName(xml_element_root, "Shader"))
		{
			const char* pixel_shader_entry = xml_element_root->Attribute("pixelshader_entry");
			const char* vertex_shader_entry = xml_element_root->Attribute("vertexshader_entry");
			const char* target_postfix = xml_element_root->Attribute("target");

			if (pixel_shader_entry && target_postfix)
			{
				//Compile shader
				char target[128] = "ps_";
				strcat_s(target, target_postfix);
				display::CompileShaderDesc compile_shader_desc;
				compile_shader_desc.code = xml_element_root->GetText();
				compile_shader_desc.entry_point = pixel_shader_entry;
				compile_shader_desc.target = target;
				if (!display::CompileShader(load_context.device, compile_shader_desc, pixel_shader))
				{
					AddError(load_context, "Error compile shader for pipeline state <%s>, errors: <%s>", load_context.name, display::GetLastErrorMessage(load_context.device));
				}
			}
			if (vertex_shader_entry && target_postfix)
			{
				//Compile shader
				char target[128] = "vs_";
				strcat_s(target, target_postfix);
				display::CompileShaderDesc compile_shader_desc;
				compile_shader_desc.code = xml_element_root->GetText();
				compile_shader_desc.entry_point = pixel_shader_entry;
				compile_shader_desc.target = target;
				if (!display::CompileShader(load_context.device, compile_shader_desc, vertex_shader))
				{
					AddError(load_context, "Error compile shader for pipeline state <%s>, errors: <%s>", load_context.name, display::GetLastErrorMessage(load_context.device));
				}
			}
		}
		else if (CheckNodeName(xml_element_root, "Rasterization"))
		{

		}

	}

	void ComputePipelineStateResource::Load(LoadContext & load_context)
	{
	}

	void DescriptorTableResource::Load(LoadContext & load_context)
	{
	}
}