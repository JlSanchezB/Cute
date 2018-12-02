#include "render_resource.h"
#include <ext/tinyxml2/tinyxml2.h>
#include "render_common.h"
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
		m_shader_resource_handle = display::CreateTextureResource(load_context.device, reinterpret_cast<void*>(&texture_buffer[0]), texture_buffer.size(), load_context.name);

		if (!m_shader_resource_handle.IsValid())
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
	auto& xml_element = load_context.current_xml_element;
	display::ConstantBufferDesc constant_buffer_desc;

	if (!QuerySizeAttribute(xml_element, "size", &constant_buffer_desc.size))
	{
		AddError(load_context, "Constant buffer <%s> needs the attribute size", load_context.name);
		return;
	}

	//Read access, by default static
	QueryEnumAttribute(xml_element, "access", &constant_buffer_desc.access, g_display_access_conversion);

	//Create constant buffer
	m_constant_buffer_handle = display::CreateConstantBuffer(load_context.device, constant_buffer_desc, load_context.name);

	if (!m_constant_buffer_handle.IsValid())
	{
		AddError(load_context, "Error creating constant buffer <%s>, display error <%>", load_context.name, display::GetLastErrorMessage(load_context.device));
	}
}

void render::RootSignatureResource::Load(LoadContext& load_context)
{
	display::RootSignatureDesc root_signature_desc;

	auto xml_element = load_context.current_xml_element->FirstChildElement();

	while (xml_element)
	{
		if (strcmp(xml_element->Name(), "RootParam") == 0)
		{
			//New root param
			auto& current_root_param = root_signature_desc.root_parameters[root_signature_desc.num_root_parameters++];

			if (!QueryEnumAttribute(xml_element, "type", &current_root_param.type, g_display_root_signature_parameter_type_conversion))
			{
				AddError(load_context, "Root signature param type is not valid for <%> root signature", load_context.name);
				return;
			}

			if (!QueryEnumAttribute(xml_element, "visibility", &current_root_param.visibility, g_display_shader_visibility_conversion))
			{
				AddError(load_context, "Root signature param visibility is not valid for <%> root signature", load_context.name);
				return;
			}
		}
		else if (strcmp(xml_element->Name(), "StaticSample") == 0)
		{

		}
		else
		{
			AddError(load_context, "Invalid xml element found <%s> in root signature <%s>", xml_element->Name(), load_context.name);
			return;
		}

		xml_element = xml_element->NextSiblingElement();
	}

	//Create root signature
}
