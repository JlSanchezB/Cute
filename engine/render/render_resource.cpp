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
	const std::pair<const char*, display::Access> g_display_access_conversion[] =
	{
		{"Static", display::Access::Static},
		{"Dynamic", display::Access::Dynamic}
	};

	const std::pair<const char*, display::RootSignatureParameterType> g_display_root_signature_parameter_type_conversion[] =
	{
		{"Constantbuffer", display::RootSignatureParameterType::ConstantBuffer},
		{"Constants", display::RootSignatureParameterType::Constants},
		{"DescriptorTable", display::RootSignatureParameterType::DescriptorTable},
		{"ShaderResource", display::RootSignatureParameterType::ShaderResource},
		{"UnorderedAccessBuffer", display::RootSignatureParameterType::UnorderAccessBuffer}
	};

	const std::pair<const char*, display::DescriptorTableParameterType> g_display_descriptor_table_parameter_type_conversion[] =
	{
		{"Constantbuffer", display::DescriptorTableParameterType::ConstantBuffer},
		{"UnorderAccessBuffer", display::DescriptorTableParameterType::UnorderAccessBuffer},
		{"ShaderResource", display::DescriptorTableParameterType::ShaderResource},
		{"Sampler", display::DescriptorTableParameterType::Sampler}
	};

	const std::pair<const char*, display::ShaderVisibility> g_display_shader_visibility_conversion[] =
	{
		{"All", display::ShaderVisibility::All},
		{"Domain", display::ShaderVisibility::Domain},
		{"Geometry", display::ShaderVisibility::Geometry},
		{"Hull", display::ShaderVisibility::Hull},
		{"Pixel", display::ShaderVisibility::Pixel},
		{"Vertex", display::ShaderVisibility::Vertex}
	};

	const std::pair<const char*, display::Filter> g_display_filter_conversion[] =
	{
		{"Point", display::Filter::Point},
		{"Linear", display::Filter::Linear},
		{"Anisotropic", display::Filter::Anisotropic}
	};

	const std::pair<const char*, display::TextureAddressMode> g_display_texture_address_mode[] =
	{
		{"Wrap", display::TextureAddressMode::Wrap},
		{"Mirror", display::TextureAddressMode::Mirror},
		{"Clamp", display::TextureAddressMode::Clamp}
	};
	
}

void render::BoolResource::Load(LoadContext& load_context)
{
	const char* value = load_context.current_xml_element->GetText();

	if (strcmp(value, "True") == 0)
	{
		m_value = true;
	}
	else if(strcmp(value, "False") == 0)
	{
		m_value = false;
	}
	else
	{
		AddError(load_context, "BoolResource <%s> doesn't have a 'True' or 'False' value", load_context.name);
	}
}

void render::TextureResource::Load(LoadContext& load_context)
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

void render::ConstantBufferResource::Load(LoadContext& load_context)
{
	AddError(load_context, "Constant buffer declaraction not supported from render passes, only game");
}

void render::RootSignatureResource::Load(LoadContext& load_context)
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
				QueryTableAttribute(load_context, xml_element_root, "type", current_root_param.type, g_display_root_signature_parameter_type_conversion, AttributeType::NonOptional);
				QueryTableAttribute(load_context, xml_element_root, "visibility", current_root_param.visibility, g_display_shader_visibility_conversion, AttributeType::Optional);

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

								QueryTableAttribute(load_context, xml_element_range, "type", range.type, g_display_descriptor_table_parameter_type_conversion, AttributeType::NonOptional);
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
				QueryTableAttribute(load_context, xml_element_root, "visibility", current_static_sampler.visibility, g_display_shader_visibility_conversion, AttributeType::Optional);
				
				QueryTableAttribute(load_context, xml_element_root, "filter", current_static_sampler.filter, g_display_filter_conversion, AttributeType::Optional);
				QueryTableAttribute(load_context, xml_element_root, "address_u", current_static_sampler.address_u, g_display_texture_address_mode, AttributeType::Optional);
				QueryTableAttribute(load_context, xml_element_root, "address_v", current_static_sampler.address_v, g_display_texture_address_mode, AttributeType::Optional);
				QueryTableAttribute(load_context, xml_element_root, "address_w", current_static_sampler.address_w, g_display_texture_address_mode, AttributeType::Optional);
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
