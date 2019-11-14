//////////////////////////////////////////////////////////////////////////
// Cute engine - Helper class for implemeting a simple render pass loader from a test
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_PASSES_LOADER_H_
#define RENDER_PASSES_LOADER_H_

#include <vector>
#include <string>
#include <array>

namespace display
{
	struct Device;
}

namespace render
{
	struct System;

	class RenderPassesLoader
	{
	public:
		bool Load(const char* filename, render::System* render_system, display::Device* device);

		//Call each frame in case it needs to reload the render passes
		void Update();

		void RenderImgui();

		bool& GetShowEditDescriptorFile()
		{
			return m_show_edit_descriptor_file;
		}

	private:
		//Last valid descriptor file
		std::vector<uint8_t> m_render_passes_descriptor_buffer;

		//Buffer used for the render passes text editor
		std::array<char, 1024 * 128> m_text_buffer = { 0 };

		//Display imgui edit descriptor file
		bool m_show_edit_descriptor_file = false;

		//Reload render passes file from the text editor
		bool m_render_system_descriptor_load_requested = false;

		//Show errors in imguid modal window
		bool m_show_errors = false;
		std::vector<std::string> m_render_system_errors;
		std::vector<std::string> m_render_system_context_errors;

		//Render system
		render::System* m_render_system = nullptr;
		
		//Device
		display::Device* m_device = nullptr;
	};
}

#endif //RENDER_PASSES_LOADER_H_