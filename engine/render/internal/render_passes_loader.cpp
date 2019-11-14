#include <render/render_passes_loader.h>
#include <render/render.h>
#include <fstream>
#include <ext/imgui/imgui.h>

namespace
{
	std::vector<uint8_t> ReadFileToBuffer(const char* file)
	{
		std::ifstream root_signature_file(file, std::ios::binary | std::ios::ate);
		if (root_signature_file.good())
		{
			std::streamsize size = root_signature_file.tellg();
			root_signature_file.seekg(0, std::ios::beg);

			std::vector<uint8_t> buffer(size);
			root_signature_file.read(reinterpret_cast<char*>(buffer.data()), size);

			return buffer;
		}
		else
		{
			return std::vector<uint8_t>(0);
		}
	}
}


bool render::RenderPassesLoader::Load(const char* filename, render::System* render_system, display::Device* device)
{
	//Read file
	m_render_passes_descriptor_buffer = ReadFileToBuffer(filename);

	if (m_render_passes_descriptor_buffer.size() > 0)
	{
		//Copy to the text editor buffer
		memcpy(m_text_buffer.data(), m_render_passes_descriptor_buffer.data(), m_render_passes_descriptor_buffer.size());
	}
	else
	{
		core::LogError("Failed to load the descriptor file %s", filename);
		return false;
	}

	m_render_system_descriptor_load_requested = true;
	m_render_system = render_system;
	m_device = device;

	Update();

	return true;
}

void render::RenderPassesLoader::Update()
{
	//Recreate the descriptor file and context if requested
	if (m_render_system_descriptor_load_requested)
	{
		//Reset errors
		m_render_system_errors.clear();

		//Load render pass sample
		size_t buffer_size = strlen(m_text_buffer.data()) + 1;

		if (!render::LoadPassDescriptorFile(m_render_system, m_device, m_text_buffer.data(), buffer_size, m_render_system_errors))
		{
			core::LogError("Failed to load the new descriptor file, reverting changes");
			m_show_errors = true;
		}

		m_render_system_descriptor_load_requested = false;
	}
}

void render::RenderPassesLoader::RenderImgui()
{
	if (m_show_edit_descriptor_file)
	{
		if (!ImGui::Begin("Render System Descriptor File", &m_show_edit_descriptor_file))
		{
			ImGui::End();
			return;
		}

		ImGui::InputTextMultiline("file", m_text_buffer.data(), m_text_buffer.size(), ImVec2(-1.0f, ImGui::GetTextLineHeight() * 32), ImGuiInputTextFlags_AllowTabInput);
		if (ImGui::Button("Reset"))
		{
			memcpy(m_text_buffer.data(), m_render_passes_descriptor_buffer.data(), m_render_passes_descriptor_buffer.size());
		}
		if (ImGui::Button("Load"))
		{
			//Request a load from the text buffer 
			m_render_system_descriptor_load_requested = true;
		}

		ImGui::End();
	}

	if (m_show_errors)
	{
		//Show modal window with the errors
		ImGui::OpenPopup("Errors loading the render pass descriptors");
		if (ImGui::BeginPopupModal("Errors loading the render pass descriptors", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			for (auto& error : m_render_system_errors)
			{
				ImGui::Text(error.c_str());
			}
			for (auto& error : m_render_system_context_errors)
			{
				ImGui::Text(error.c_str());
			}
			ImGui::Separator();

			if (ImGui::Button("OK", ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
				m_show_errors = false;
			}
			ImGui::EndPopup();
		}
	}
}
