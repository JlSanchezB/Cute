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

	void OnInit() override
	{
		display::DeviceInitParams device_init_params;

		device_init_params.debug = true;
		device_init_params.width = kInitWidth;
		device_init_params.height = kInitHeight;
		device_init_params.num_frames = 2;

		m_device = display::CreateDevice(device_init_params);
	}
	void OnDestroy() override
	{
		display::DestroyDevice(m_device);
	}
	void OnTick() override
	{
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	HelloWorldGame hello_world_game;

	return platform::Run("Hello world", param, HelloWorldGame::kInitWidth, HelloWorldGame::kInitHeight, &hello_world_game);
}