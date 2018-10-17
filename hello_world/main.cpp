#include <platform.h>
#include <display.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>


class HelloWorldGame : public platform::Game
{
public:
	const static size_t kInitWidth = 500;
	const static size_t kInitHeight = 500;

	display::Device* m_device;

	display::CommandListHandle m_command_list;

	void OnInit() override
	{
		display::DeviceInitParams device_init_params;

		device_init_params.debug = true;
		device_init_params.width = kInitWidth;
		device_init_params.height = kInitHeight;
		device_init_params.num_frames = 2;

		m_device = display::CreateDevice(device_init_params);

		m_command_list = display::CreateCommandList(m_device);
	}
	void OnDestroy() override
	{
		display::DestroyDevice(m_device);
	}
	void OnTick() override
	{
		display::Context context = { m_device, m_command_list };

		display::BeginFrame(m_device);

		//Open command list
		display::OpenCommandList(m_device, m_command_list);

		//Set BackBuffer
		display::RenderTargetHandle back_buffer = display::GetBackBuffer(m_device);
		display::SetRenderTargets(&context, 1, &back_buffer, nullptr);

		//Clear
		const float clear_colour[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		display::ClearRenderTargetColour(&context, back_buffer, clear_colour);

		//Close command list
		display::CloseCommandList(m_device, m_command_list);

		//Execute command list


		//Present
		display::Present(m_device);

		display::EndFrame(m_device);
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	HelloWorldGame hello_world_game;

	return platform::Run("Hello world", param, HelloWorldGame::kInitWidth, HelloWorldGame::kInitHeight, &hello_world_game);
}