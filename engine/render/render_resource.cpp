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
		AddError(load_context, "BoolResource doesn't have a 'True' or 'False' value");
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
		m_shader_resource_handle = display::CreateTextureResource(load_context.device, reinterpret_cast<void*>(&texture_buffer[0]), texture_buffer.size(), texture_filename);

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
}

void render::RootSignatureResource::Load(LoadContext& load_context)
{
}
