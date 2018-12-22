#include <core/platform.h>
#include <display/display.h>
#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>

namespace
{
	std::vector<char> ReadFileToBuffer(const char* file)
	{
		std::ifstream root_signature_file(file, std::ios::binary | std::ios::ate);
		if (root_signature_file.good())
		{
			std::streamsize size = root_signature_file.tellg();
			root_signature_file.seekg(0, std::ios::beg);

			std::vector<char> buffer(size);
			root_signature_file.read(buffer.data(), size);

			return buffer;
		}
		else
		{
			return std::vector<char>(0);
		}
	}
}

class RenderPassesGame : public platform::Game
{
public:
	constexpr static uint32_t kInitWidth = 500;
	constexpr static uint32_t kInitHeight = 500;

	uint32_t m_width;
	uint32_t m_height;

	display::Device* m_device;

	render::System* m_render_pass_system = nullptr;

	render::RenderContext* m_render_context = nullptr;

	void OnInit() override
	{
		display::DeviceInitParams device_init_params;

		device_init_params.debug = true;
		device_init_params.width = kInitWidth;
		device_init_params.height = kInitHeight;
		device_init_params.tearing = true;
		device_init_params.vsync = false;
		device_init_params.num_frames = 3;

		m_device = display::CreateDevice(device_init_params);

		if (m_device != nullptr)
		{
			std::runtime_error::exception("Error creating the display device");
		}

		SetDevice(m_device);

		//Create constant buffer with the time
		display::ConstantBufferHandle game_constant_buffer;
		display::ConstantBufferDesc constant_buffer_desc;
		constant_buffer_desc.access = display::Access::Dynamic;
		constant_buffer_desc.size = 16;
		game_constant_buffer = display::CreateConstantBuffer(m_device, constant_buffer_desc, "GameConstantBuffer");

		//Create render pass system
		m_render_pass_system = render::CreateRenderPassSystem();

		//Load render pass sample
		std::vector<std::string> errors;
		if (render::LoadPassDescriptorFile(m_render_pass_system, m_device, "render_pass_sample.xml", errors))
		{
			//Create pass
			render::ResourceMap init_resource_map;
			init_resource_map["GameGlobal"] = CreateResourceFromHandle<render::ConstantBufferResource>(game_constant_buffer);
			init_resource_map["BackBuffer"] = CreateResourceFromHandle<render::RenderTargetReferenceResource>(display::GetBackBuffer(m_device));

			render::RenderContext::PassInfo pass_info;
			pass_info.width = m_width;
			pass_info.height = m_height;

			m_render_context = render::CreateRenderContext(m_render_pass_system, m_device, "Main", pass_info, init_resource_map, errors);
		}
	}
	void OnDestroy() override
	{
		if (m_render_pass_system)
		{
			if (m_render_context)
			{
				render::DestroyRenderContext(m_render_pass_system, m_render_context);
			}

			render::DestroyRenderPassSystem(m_render_pass_system, m_device);
		}



		display::DestroyDevice(m_device);
	}
	void OnTick(double total_time, float elapsed_time) override
	{
		display::BeginFrame(m_device);

		//Update time
		render::ConstantBufferResource* game_global_resource = m_render_context->GetResource<render::ConstantBufferResource>("GameGlobal");

		struct GameConstantBuffer
		{
			float time[4];
		};
		GameConstantBuffer game_constant_buffer = { { static_cast<float>(total_time), elapsed_time, 0.f, 0.f } };
		
		//Update game constant buffer
		display::UpdateResourceBuffer(m_device, game_global_resource->GetHandle(), &game_constant_buffer, sizeof(game_constant_buffer));

		//Capture pass
		render::CaptureRenderContext(m_render_pass_system, m_render_context);
		//Execute pass
		render::ExecuteRenderContext(m_render_pass_system, m_render_context);

		display::EndFrame(m_device);
	}

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override
	{
		m_width = width;
		m_height = height;

		if (m_render_context)
		{
			render::RenderContext::PassInfo pass_info = m_render_context->GetPassInfo();
			pass_info.width = m_width;
			pass_info.height = m_height;
			m_render_context->UpdatePassInfo(pass_info);
		}
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	RenderPassesGame render_passes_game;

	return platform::Run("Render Pass Test", param, RenderPassesGame::kInitWidth, RenderPassesGame::kInitHeight, &render_passes_game);
}