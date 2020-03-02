#include <core/platform.h>
#include <display/display.h>
#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include <render/render_passes_loader.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>

class RenderPassesGame : public platform::Game
{
public:
	constexpr static uint32_t kInitWidth = 500;
	constexpr static uint32_t kInitHeight = 500;

	uint32_t m_width;
	uint32_t m_height;

	display::Device* m_device;

	render::System* m_render_pass_system = nullptr;

	//Render passes loader
	render::RenderPassesLoader m_render_passes_loader;

	//Game constant buffer
	display::ConstantBufferHandle m_game_constant_buffer;

	//Show errors in imguid modal window
	bool m_show_errors = false;
	std::vector<std::string> m_render_system_errors;
	std::vector<std::string> m_render_system_context_errors;

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

		if (m_device == nullptr)
		{
			throw std::runtime_error::exception("Error creating the display device");
		}

		SetDevice(m_device);

		//Create constant buffer with the time
		display::ConstantBufferDesc constant_buffer_desc;
		constant_buffer_desc.access = display::Access::Dynamic;
		constant_buffer_desc.size = 16;
		m_game_constant_buffer = display::CreateConstantBuffer(m_device, constant_buffer_desc, "GameConstantBuffer");

		//Create render pass system
		m_render_pass_system = render::CreateRenderSystem(m_device, nullptr, this);

		render::AddGameResource(m_render_pass_system, "GameGlobal"_sh32, CreateResourceFromHandle<render::ConstantBufferResource>(display::WeakConstantBufferHandle(m_game_constant_buffer)));
		render::AddGameResource(m_render_pass_system, "BackBuffer"_sh32, CreateResourceFromHandle<render::RenderTargetResource>(display::GetBackBuffer(m_device), m_width, m_height));

		//Load render passes descriptor
		m_render_passes_loader.Load("render_pass_sample.xml", m_render_pass_system, m_device);

	}
	void OnDestroy() override
	{
		if (m_render_pass_system)
		{
			render::DestroyRenderSystem(m_render_pass_system, m_device);
		}
		
		//Destroy handles
		display::DestroyHandle(m_device, m_game_constant_buffer);

		display::DestroyDevice(m_device);
	}
	void OnTick(double total_time, float elapsed_time) override
	{
		render::BeginPrepareRender(m_render_pass_system);

		{
			//Check if the render passes loader needs to load
			m_render_passes_loader.Update();
		}

		//Update time
		struct GameConstantBuffer
		{
			float time[4];
		};
		GameConstantBuffer game_constant_buffer = { { static_cast<float>(total_time), elapsed_time, 0.f, 0.f } };
		
		//Update game constant buffer
		display::UpdateResourceBuffer(m_device, m_game_constant_buffer, &game_constant_buffer, sizeof(game_constant_buffer));

		render::PassInfo pass_info;

		auto& render_frame = render::GetGameRenderFrame(m_render_pass_system);

		render_frame.AddRenderPass("Main"_sh32, 0, pass_info);
		render_frame.AddRenderPass("RenderToRenderTarget"_sh32, 0, pass_info);

		render::EndPrepareRenderAndSubmit(m_render_pass_system);
	}

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override
	{
		m_width = width;
		m_height = height;

		if (m_render_pass_system)
		{
			render::GetResource<render::RenderTargetResource>(m_render_pass_system, "BackBuffer"_sh32)->UpdateInfo(width, height);
		}
	}

	void OnAddImguiMenu() override
	{
		//Add menu for modifying the render system descriptor file
		if (ImGui::BeginMenu("RenderSystem"))
		{
			m_render_passes_loader.GetShowEditDescriptorFile() = ImGui::MenuItem("Edit descriptor file");
			ImGui::EndMenu();
		}
	}

	void OnImguiRender() override
	{
		m_render_passes_loader.RenderImgui();

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
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	RenderPassesGame render_passes_game;

	return platform::Run("Render Pass Test", param, RenderPassesGame::kInitWidth, RenderPassesGame::kInitHeight, &render_passes_game);
}