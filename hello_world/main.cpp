#include <platform.h>
#include <display.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>

class HelloWorldGame : public platform::Game
{
public:
	const static size_t kInitWidth = 500;
	const static size_t kInitHeight = 500;

	display::Device* m_device;

	display::CommandListHandle m_command_list;

	display::RootSignatureHandle m_root_signature;

	void OnInit() override
	{
		display::DeviceInitParams device_init_params;

		device_init_params.debug = true;
		device_init_params.width = kInitWidth;
		device_init_params.height = kInitHeight;
		device_init_params.num_frames = 2;

		m_device = display::CreateDevice(device_init_params);

		m_command_list = display::CreateCommandList(m_device);

		{
			std::ifstream root_signature_file("root_signature.fxo", std::ios::binary | std::ios::ate);
			if (root_signature_file.good())
			{
				std::streamsize size = root_signature_file.tellg();
				root_signature_file.seekg(0, std::ios::beg);

				std::vector<char> buffer(size);
				root_signature_file.read(buffer.data(), size);

				if (root_signature_file.good())
				{
					//Create the root signature
					m_root_signature = display::CreateRootSignature(m_device, reinterpret_cast<void*>(buffer.data()), size);
				}
			}
		}
		
	}
	void OnDestroy() override
	{
		//Free handles
		display::DestroyCommandList(m_device, m_command_list);
		display::DestroyRootSignature(m_device, m_root_signature);

		display::DestroyDevice(m_device);
	}
	void OnTick() override
	{

		display::BeginFrame(m_device);

		//Open command list
		display::OpenCommandList(m_device, m_command_list);

		//Set BackBuffer
		display::RenderTargetHandle back_buffer = display::GetBackBuffer(m_device);
		display::SetRenderTargets(m_device, m_command_list, 1, &back_buffer, nullptr);

		//Clear
		const float clear_colour[] = { rand() / (RAND_MAX + 1.f) , 0.2f, 0.4f, 1.0f };
		display::ClearRenderTargetColour(m_device, m_command_list, back_buffer, clear_colour);

		//Close command list
		display::CloseCommandList(m_device, m_command_list);

		//Execute command list
		display::ExecuteCommandList(m_device, m_command_list);

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