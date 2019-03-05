#include <core/platform.h>
#include <display/display.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <chrono>
#include <core/imgui_render.h>
#include <core/string_hash.h>

#define MICROPROFILE_MAX_FRAME_HISTORY (2<<10)
#define MICROPROFILE_IMPL
#include <ext/microprofile/microprofile.h>

namespace display
{
	//Size change
	void ChangeWindowSize(Device* device, size_t width, size_t height, bool minimized);

	//Only need to control the resolution from the platform layer if tearing is enabled
	bool IsTearingEnabled(Device* device);

	bool GetCurrentDisplayRect(Device* device, Rect& rect);

	void DisplayImguiStats(Device* device, bool* activated);
}

namespace core
{
	//Render Imgui Log
	bool LogRender();
}

namespace
{
	//Device
	display::Device* g_device = nullptr;

	//Fullscreen
	RECT g_window_rect;
	UINT g_window_style = WS_OVERLAPPEDWINDOW;
	bool g_windowed = true;

	//global variable with the current hwnd, needed for specicif win32 and dx12 init
	HWND g_current_hwnd;

	//Frecuency for the perfomance timer
	LARGE_INTEGER g_frequency;

	//Frecuency for the perfomance timer
	LARGE_INTEGER g_current_time;

	//Begin time
	LARGE_INTEGER g_begin_time;

	//Total accumulated time
	double m_total_time = 0.f;

	//Imgui menu enabled
	bool g_imgui_menu_enable = false;

	//Imgui fps enable
	bool g_imgui_fps_enable = true;

	//Imgui demo
	bool g_imgui_demo_enable = false;

	//Imgui display stats
	bool g_imgui_display_stats = false;

	//Imgui render log
	bool g_imgui_log_enable = false;

	constexpr std::array<platform::InputSlot, 256> build_keyboard_conversion()
	{
		std::array<platform::InputSlot, 256> table = {platform::InputSlot::Invalid};

		table[VK_BACK] = platform::InputSlot::Back;
		table[VK_TAB] = platform::InputSlot::Tab;
		table[VK_RETURN] = platform::InputSlot::Return;
		table[VK_LSHIFT] = platform::InputSlot::LShift;
		table[VK_LCONTROL] = platform::InputSlot::LControl;
		table[VK_RSHIFT] = platform::InputSlot::RShift;
		table[VK_RCONTROL] = platform::InputSlot::RControl;
		table[VK_ESCAPE] = platform::InputSlot::Escape;
		table[VK_SPACE] = platform::InputSlot::Space;
		table[VK_LEFT] = platform::InputSlot::Left;
		table[VK_UP] = platform::InputSlot::Up;
		table[VK_DOWN] = platform::InputSlot::Down;
		table[VK_RIGHT] = platform::InputSlot::Right;
		table[VK_PRIOR] = platform::InputSlot::PageUp;
		table[VK_NEXT] = platform::InputSlot::PageDown;

		return table;
	}

	//Table for converting from VK values to platform enum
	std::array<platform::InputSlot,256> g_keyboard_conversion = build_keyboard_conversion();

	//Input state
	std::array<bool, static_cast<size_t>(platform::InputSlot::Count)> g_input_slot_state = { false };

	//Input events
	std::vector<platform::InputEvent> g_input_events;

	//Windows message handle
	LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (imgui_render::WndProcHandler(hWnd, message, wParam, lParam))
			return true;

		platform::Game* game = reinterpret_cast<platform::Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

		switch (message)
		{
		case WM_CREATE:
			{
				// Save the DXSample* passed in to CreateWindow.
				LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
				SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
			}
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_SIZE:
			{
				RECT windowRect = {};
				GetWindowRect(hWnd, &windowRect);

				RECT clientRect = {};
				GetClientRect(hWnd, &clientRect);

				core::LogInfo("Windows is going to change size (%i,%i)", clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

				//Call the device
				if (g_device)
				{
					display::ChangeWindowSize(g_device, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
				}
				//Call the game
				game->OnSizeChange(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
			}
			return 0;
		case WM_KEYDOWN:
		{
			if (wParam == VK_OEM_8)
			{
				g_imgui_menu_enable = ! g_imgui_menu_enable;
			}
			else
			{
				if (g_keyboard_conversion[wParam] != platform::InputSlot::Invalid)
				{
					g_input_slot_state[static_cast<size_t>(g_keyboard_conversion[wParam])] = true;
					g_input_events.push_back({ platform::EventType::KeyDown, g_keyboard_conversion[wParam] });
				}
			}
			break;
		}
		case WM_KEYUP:
		{
			if (g_keyboard_conversion[wParam] != platform::InputSlot::Invalid)
			{
				g_input_slot_state[static_cast<size_t>(g_keyboard_conversion[wParam])] = false;
				g_input_events.push_back({ platform::EventType::KeyUp, g_keyboard_conversion[wParam] });
			}
			break;
		}
		case WM_SYSKEYDOWN:
			// Handle ALT+ENTER:
			if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
			{
				if (game && g_device && display::IsTearingEnabled(g_device))
				{
					if (g_windowed)
					{
						core::LogInfo("Windows is going to full screen");
						//Change to full screen

						// Save the old window rect so we can restore it when exiting fullscreen mode.
						GetWindowRect(hWnd, &g_window_rect);

						// Make the window borderless so that the client area can fill the screen.
						SetWindowLong(hWnd, GWL_STYLE, g_window_style & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

						//Get current display size
						RECT fullscreenWindowRect;
						display::Rect rect;
						if (display::GetCurrentDisplayRect(g_device, rect))
						{
							fullscreenWindowRect.bottom = static_cast<LONG>(rect.bottom);
							fullscreenWindowRect.top = static_cast<LONG>(rect.top);
							fullscreenWindowRect.left = static_cast<LONG>(rect.left);
							fullscreenWindowRect.right = static_cast<LONG>(rect.right);
						}
						else
						{
							// Get the settings of the primary display
							DEVMODE devMode = {};
							devMode.dmSize = sizeof(DEVMODE);
							EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

							fullscreenWindowRect = {
								devMode.dmPosition.x,
								devMode.dmPosition.y,
								devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
								devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
							};
						}

						SetWindowPos(
							hWnd,
							HWND_TOPMOST,
							fullscreenWindowRect.left,
							fullscreenWindowRect.top,
							fullscreenWindowRect.right,
							fullscreenWindowRect.bottom,
							SWP_FRAMECHANGED | SWP_NOACTIVATE);


						ShowWindow(hWnd, SW_MAXIMIZE);

						g_windowed = false;
					}
					else
					{
						core::LogInfo("Windows is restoring size");

						// Restore the window's attributes and size.
						SetWindowLong(hWnd, GWL_STYLE, g_window_style);

						SetWindowPos(
							hWnd,
							HWND_NOTOPMOST,
							g_window_rect.left,
							g_window_rect.top,
							g_window_rect.right - g_window_rect.left,
							g_window_rect.bottom - g_window_rect.top,
							SWP_FRAMECHANGED | SWP_NOACTIVATE);

						ShowWindow(hWnd, SW_NORMAL);

						g_windowed = true;
					}
					return 0;
				}
			}
			// Send all other WM_SYSKEYDOWN messages to the default WndProc.
			break;
		}

		// Handle any messages the switch statement didn't.
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	void RenderFPSOverlay(float elapsed_time)
	{

		const float distance = 10.0f;
		static int corner = 3;
		ImVec2 window_pos = ImVec2((corner & 1) ? ImGui::GetIO().DisplaySize.x - distance : distance, (corner & 2) ? ImGui::GetIO().DisplaySize.y - distance : distance);
		ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
		if (corner != -1)
			ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
		ImGui::SetNextWindowBgAlpha(0.3f); // Transparent background
		bool open = true;
		if (ImGui::Begin("Perf", &open, (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
		{
			ImGui::Text("FPS: %2.1f", 1.f / elapsed_time);
		}
		ImGui::End();
	}

	void RenderImgui(platform::Game* game, float elapsed_time)
	{
		if (g_imgui_menu_enable)
		{
			//Display menu
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("Cute"))
				{
					if (ImGui::MenuItem("Logger")) {};
					ImGui::Checkbox("Show FPS", &g_imgui_fps_enable);
					ImGui::Checkbox("Show Log", &g_imgui_log_enable);
					ImGui::Checkbox("Show Imgui Demo", &g_imgui_demo_enable);
					ImGui::Checkbox("Display Stats", &g_imgui_display_stats);
					ImGui::EndMenu();
				}
				//Call game to add it owns menus
				game->OnAddImguiMenu();

				ImGui::EndMainMenuBar();
			}
		}

		if (g_imgui_demo_enable)
		{
			//Show Imgui demo
			ImGui::ShowDemoWindow(&g_imgui_demo_enable);
		}

		if (g_imgui_fps_enable)
		{
			RenderFPSOverlay(elapsed_time);
		}

		if (g_imgui_display_stats && g_device)
		{
			display::DisplayImguiStats(g_device, &g_imgui_display_stats);
		}

		if (g_imgui_log_enable)
		{
			g_imgui_log_enable = core::LogRender();
		}

		//Render game imgui
		game->OnImguiRender();
	}
}

namespace platform
{
	HWND GetHwnd()
	{
		return g_current_hwnd;
	}

	//Called from the device before present with the present command list
	void PresentCallback(display::Context* context)
	{
		imgui_render::Draw(context);
	}

	void Game::SetDevice(display::Device * device)
	{
		g_device = device;

		//Create IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		//Init imgui
		imgui_render::Init(g_current_hwnd);

		//Create all resources for imgui
		imgui_render::CreateResources(device);
	}

	bool Game::GetInputSlotState(InputSlot input_slot) const
	{
		return g_input_slot_state[static_cast<size_t>(input_slot)];
	}

	const std::vector<InputEvent> Game::GetInputEvents() const
	{
		return g_input_events;
	}

	char Run(const char* name, void* param, uint32_t width, uint32_t height, Game* game)
	{
#ifdef _STRING_HASH_MAP_ENABLED_
		core::CreateStringHashMap();
#endif

		HINSTANCE hInstance = *(reinterpret_cast<HINSTANCE*>(param));

		// Initialize the window class.
		WNDCLASSEX windowClass = { 0 };
		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = hInstance;
		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		windowClass.lpszClassName = name;
		RegisterClassEx(&windowClass);

		RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		// Create the window and store a handle to it.
		g_current_hwnd = CreateWindow(
			windowClass.lpszClassName,
			name,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			nullptr,		// We have no parent window.
			nullptr,		// We aren't using menus.
			hInstance,
			game);

		ShowWindow(g_current_hwnd, true);

		//Init microprofiler
		MicroProfileOnThreadCreate("Main");
		MicroProfileSetForceEnable(true);
		MicroProfileSetEnableAllGroups(true);
		MicroProfileSetForceMetaCounters(true);

		MicroProfileWebServerStart();
		
		//Init callback
		game->OnInit();

		//Calculate frecuency for timer
		QueryPerformanceFrequency(&g_frequency);
		QueryPerformanceCounter(&g_current_time);
		QueryPerformanceCounter(&g_begin_time);

		// Main sample loop.
		MSG msg = {};
		do 
		{
			// Process any messages in the queue.
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					break;
				}
				else
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			//Calculate time
			LARGE_INTEGER last_time = g_current_time;
			QueryPerformanceCounter(&g_current_time);
			float elapsed_time = static_cast<float>(g_current_time.QuadPart - last_time.QuadPart) / g_frequency.QuadPart;
			if (elapsed_time > 0.5f)
			{
				core::LogInfo("Timestep was really high (Debugging?), limited to 30fps");
				elapsed_time = 1.f / 30.f;
			}
			m_total_time += elapsed_time;
			
			//New frame for imgui
			imgui_render::NextFrame(g_current_hwnd, elapsed_time);

			{
				MICROPROFILE_SCOPEI("Platform", "GameTick", 0xFFFF00FF);
				//Render
				game->OnTick(m_total_time, elapsed_time);
			}

			{
				MICROPROFILE_SCOPEI("Platform", "BuildImgui", 0xFFFF00FF);
				//Render platform imgui (menu, fps,...)
				RenderImgui(game, elapsed_time);
			}

			{
				MICROPROFILE_SCOPEI("Platform", "RenderImgui", 0xFFFF00FF);
				//Render IMGUI
				ImGui::Render();
			}

			{
				MICROPROFILE_SCOPEI("Platform", "Present", 0xFFFF00FF);
				//Present
				display::Present(g_device);
			}

			{
				//Clear input events
				g_input_events.clear();
			}

			MicroProfileFlip();

		} while (true);

		core::LogInfo("Closing game");

		//Destroy all imgui resources
		imgui_render::DestroyResources(g_device);
		//Destroy Imgui
		ImGui::DestroyContext();

		//Destroy callback
		game->OnDestroy();

#ifdef _STRING_HASH_MAP_ENABLED_
		core::DestroyStringHashMap();
#endif

		MicroProfileWebServerStop();
		MicroProfileShutdown();

		// Return this part of the WM_QUIT message to Windows.
		return static_cast<char>(msg.wParam);
	}
}