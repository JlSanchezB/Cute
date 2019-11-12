#include <core/platform.h>
#include <display/display.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <Xinput.h>
#include <chrono>
#include <core/imgui_render.h>
#include <core/string_hash.h>
#include <core/profile.h>

typedef unsigned __int64 QWORD;

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
	//Clone the data for deferred rendering of the imgui
	struct ImguiCloneFrameData
	{
		//Container, we need to control the command list
		ImDrawData draw_data;

		void Capture()
		{
			Clear();

			ImDrawData* source = ImGui::GetDrawData();

			//Copy the data
			draw_data = *source;

			//Allocate space for the command lists
			draw_data.CmdLists = new ImDrawList*[draw_data.CmdListsCount];

			//Close all command lists
			for (int i = 0; i < draw_data.CmdListsCount; i++)
			{
				draw_data.CmdLists[i] = source->CmdLists[i]->CloneOutput();
			}
		}

		void Clear()
		{
			for (int i = 0; i < draw_data.CmdListsCount; i++)
			{
				delete draw_data.CmdLists[i];
			}

			delete draw_data.CmdLists;
			draw_data.CmdLists = nullptr;
		}

		~ImguiCloneFrameData()
		{
			Clear();
		}
	};

	struct Platform
	{
		//Device
		display::Device* m_device = nullptr;

		//Fullscreen
		RECT m_window_rect;
		UINT m_window_style = WS_OVERLAPPEDWINDOW;
		bool m_windowed = true;

		//global variable with the current hwnd, needed for specicif win32 and dx12 init
		HWND m_current_hwnd;

		//Frecuency for the perfomance timer
		LARGE_INTEGER m_frequency;

		//Frecuency for the perfomance timer
		LARGE_INTEGER m_current_time;

		//Begin time
		LARGE_INTEGER m_begin_time;

		//Total accumulated time
		double m_total_time = 0.f;

		//Last elapsed time
		float m_last_elapsed_time = 0.f;

		//Imgui menu enabled
		bool m_imgui_menu_enable = false;

		//Imgui fps enable
		bool m_imgui_fps_enable = true;

		//Imgui demo
		bool m_imgui_demo_enable = false;

		//Imgui display stats
		bool m_imgui_display_stats = false;

		//Imgui render log
		bool m_imgui_log_enable = false;

		constexpr std::array<platform::InputSlotState, 256> build_keyboard_conversion()
		{
			std::array<platform::InputSlotState, 256> table{};
			
			for (auto& value : table) value = platform::InputSlotState::Invalid;

			table[VK_BACK] = platform::InputSlotState::Back;
			table[VK_TAB] = platform::InputSlotState::Tab;
			table[VK_RETURN] = platform::InputSlotState::Return;
			table[VK_LSHIFT] = platform::InputSlotState::LShift;
			table[VK_LCONTROL] = platform::InputSlotState::LControl;
			table[VK_RSHIFT] = platform::InputSlotState::RShift;
			table[VK_RCONTROL] = platform::InputSlotState::RControl;
			table[VK_ESCAPE] = platform::InputSlotState::Escape;
			table[VK_SPACE] = platform::InputSlotState::Space;
			table[VK_LEFT] = platform::InputSlotState::Left;
			table[VK_UP] = platform::InputSlotState::Up;
			table[VK_DOWN] = platform::InputSlotState::Down;
			table[VK_RIGHT] = platform::InputSlotState::Right;
			table[VK_PRIOR] = platform::InputSlotState::PageUp;
			table[VK_NEXT] = platform::InputSlotState::PageDown;

			return table;
		}

		//Table for converting from VK values to platform enum
		std::array<platform::InputSlotState, 256> m_keyboard_conversion = build_keyboard_conversion();

		//Input state
		std::array<bool, static_cast<size_t>(platform::InputSlotState::Count)> m_input_slot_state = { false };

		//Input values
		std::array<float, static_cast<size_t>(platform::InputSlotValue::Count)> m_input_slot_values = { 0.f };

		//Input events
		std::vector<platform::InputEvent> m_input_events;

		//Index of the update frame, used for syncs
		size_t m_update_frame_index = 0;
		//Index of the render thread, used for syncs
		size_t m_render_frame_index = 1;

		//Number of frames saved for imgui
		static constexpr size_t kNumImguiFrames = 5;

		std::array <ImguiCloneFrameData, kNumImguiFrames> m_imgui_draw_data;
	};
	
	//Global platform access
	Platform* g_Platform;

	void InitInput()
	{
		//UINT nDevices;
		//PRAWINPUTDEVICELIST pRawInputDeviceList;
		//GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST));
		//pRawInputDeviceList = (PRAWINPUTDEVICELIST)alloca(sizeof(RAWINPUTDEVICELIST) * nDevices);
		//GetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST));

		RAWINPUTDEVICE* devices = (RAWINPUTDEVICE*)alloca(sizeof(RAWINPUTDEVICE) * 2);
		devices[0].usUsagePage = 1;
		devices[0].usUsage = 6;
		devices[0].dwFlags = RIDEV_INPUTSINK;
		devices[0].hwndTarget = g_Platform->m_current_hwnd;
		devices[1].usUsagePage = 1;
		devices[1].usUsage = 2;
		devices[1].dwFlags = RIDEV_INPUTSINK;
		devices[1].hwndTarget = g_Platform->m_current_hwnd;
		RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));
	}

	void ClearInput()
	{
		//Clear input as the window has been deactivated

		for (auto& key : g_Platform->m_input_slot_state)
		{
			key = false;
		}
	}

	void ProcessInputEvent(RAWINPUT& input_event)
	{
		if (input_event.header.dwType == RIM_TYPEKEYBOARD)
		{
			auto& keyboard = input_event.data.keyboard;

			if (keyboard.VKey == VK_OEM_8 && keyboard.Message == WM_KEYDOWN)
			{
				g_Platform->m_imgui_menu_enable = !g_Platform->m_imgui_menu_enable;
			}
			else
			{
				auto key = g_Platform->m_keyboard_conversion[keyboard.VKey];
				if (key != platform::InputSlotState::Invalid)
				{
					if (keyboard.Message == WM_KEYUP)
					{
						g_Platform->m_input_slot_state[static_cast<size_t>(key)] = false;
						g_Platform->m_input_events.push_back({ platform::EventType::KeyUp, key });
					}
					if (keyboard.Message == WM_KEYDOWN)
					{
						g_Platform->m_input_slot_state[static_cast<size_t>(key)] = true;
						g_Platform->m_input_events.push_back({ platform::EventType::KeyDown, key });
					}
				}
			}
		}
		else if (input_event.header.dwType == RIM_TYPEMOUSE)
		{
			auto& mouse = input_event.data.mouse;

			if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
			{
				g_Platform->m_input_slot_state[static_cast<size_t>(platform::InputSlotState::LeftMouseButton)] = true;
				g_Platform->m_input_events.push_back({ platform::EventType::KeyDown, platform::InputSlotState::LeftMouseButton });
			}
			if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
			{
				g_Platform->m_input_slot_state[static_cast<size_t>(platform::InputSlotState::LeftMouseButton)] = false;
				g_Platform->m_input_events.push_back({ platform::EventType::KeyUp, platform::InputSlotState::LeftMouseButton });
			}
			if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
			{
				g_Platform->m_input_slot_state[static_cast<size_t>(platform::InputSlotState::MiddleMouseButton)] = true;
				g_Platform->m_input_events.push_back({ platform::EventType::KeyDown, platform::InputSlotState::MiddleMouseButton });
			}
			if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
			{
				g_Platform->m_input_slot_state[static_cast<size_t>(platform::InputSlotState::MiddleMouseButton)] = false;
				g_Platform->m_input_events.push_back({ platform::EventType::KeyUp, platform::InputSlotState::MiddleMouseButton });
			}
			if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
			{
				g_Platform->m_input_slot_state[static_cast<size_t>(platform::InputSlotState::RightMouseButton)] = true;
				g_Platform->m_input_events.push_back({ platform::EventType::KeyDown, platform::InputSlotState::RightMouseButton });
			}
			if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
			{
				g_Platform->m_input_slot_state[static_cast<size_t>(platform::InputSlotState::RightMouseButton)] = false;
				g_Platform->m_input_events.push_back({ platform::EventType::KeyUp, platform::InputSlotState::RightMouseButton });
			}
			if (mouse.usButtonFlags & RI_MOUSE_WHEEL)
			{
				float delta = static_cast<float>((short)(mouse.usButtonData)) / static_cast<float>(WHEEL_DELTA);
				g_Platform->m_input_events.push_back({ platform::EventType::MouseWheel, delta });
			}

			if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
			{
				g_Platform->m_input_slot_values[static_cast<size_t>(platform::InputSlotValue::MousePositionX)] = static_cast<float>(mouse.lLastX);
				g_Platform->m_input_slot_values[static_cast<size_t>(platform::InputSlotValue::MousePositionY)] = static_cast<float>(mouse.lLastY);
			}
			else
			{
				g_Platform->m_input_slot_values[static_cast<size_t>(platform::InputSlotValue::MouseRelativePositionX)] += static_cast<float>(mouse.lLastX);
				g_Platform->m_input_slot_values[static_cast<size_t>(platform::InputSlotValue::MouseRelativePositionY)] += static_cast<float>(mouse.lLastY);
			}
		}
	}

	void InputFrameInit()
	{
		//Clear input events
		g_Platform->m_input_events.clear();

		g_Platform->m_input_slot_values[static_cast<size_t>(platform::InputSlotValue::MouseRelativePositionX)] = 0.f;
		g_Platform->m_input_slot_values[static_cast<size_t>(platform::InputSlotValue::MouseRelativePositionY)] = 0.f;
	}

	void ProcessControllerButton(bool key, platform::InputSlotState controller_button)
	{
		auto& state = g_Platform->m_input_slot_state[static_cast<size_t>(controller_button)];
		if (state != key)
		{
			state = key;
			g_Platform->m_input_events.push_back({ (state) ? platform::EventType::KeyUp : platform::EventType::KeyDown, controller_button });
		}
	}

	void ProcessControllerThumb(SHORT value, SHORT deadzone, platform::InputSlotValue slot)
	{
		if (value > deadzone)
		{
			g_Platform->m_input_slot_values[static_cast<size_t>(slot)] = static_cast<float>(value) / 32767.f;
		}
		else if (value < -deadzone)
		{
			g_Platform->m_input_slot_values[static_cast<size_t>(slot)] = static_cast<float>(value) / 32768.f;
		}
		else
		{
			g_Platform->m_input_slot_values[static_cast<size_t>(slot)] = 0.f;
		}
	}

	void CaptureInput()
	{
		//Capture Raw Input

		//Get number of blocks
		UINT block_size = 0;
		GetRawInputBuffer(NULL, &block_size, sizeof(RAWINPUTHEADER));
		block_size *= 8;

		if (block_size > 0)
		{
			PRAWINPUT raw_input_blocks = (PRAWINPUT)alloca(block_size);

			//Capture blocks
			UINT block_size_2 = block_size;
			UINT block_count = GetRawInputBuffer(raw_input_blocks, &block_size_2, sizeof(RAWINPUTHEADER));

			if (block_count != -1)
			{
				//Process blocks
				for (UINT i = 0; i < block_count; ++i)
				{
					ProcessInputEvent(*raw_input_blocks);

					NEXTRAWINPUTBLOCK(raw_input_blocks);
				}
			}
		}

		//Capture XInput
		XINPUT_STATE xinput_state;
		if (XInputGetState(0, &xinput_state) == ERROR_SUCCESS)
		{
			ProcessControllerThumb(xinput_state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, platform::InputSlotValue::ControllerThumbLeftX);
			ProcessControllerThumb(xinput_state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, platform::InputSlotValue::ControllerThumbLeftY);
			ProcessControllerThumb(xinput_state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, platform::InputSlotValue::ControllerThumbRightX);
			ProcessControllerThumb(xinput_state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, platform::InputSlotValue::ControllerThumbRightY);

			g_Platform->m_input_slot_values[static_cast<size_t>(platform::InputSlotValue::ControllerLeftTrigger)] = static_cast<float>(xinput_state.Gamepad.bLeftTrigger) / 255.f;
			g_Platform->m_input_slot_values[static_cast<size_t>(platform::InputSlotValue::ControllerRightTrigger)] = static_cast<float>(xinput_state.Gamepad.bRightTrigger) / 255.f;

			ProcessControllerButton(xinput_state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD, platform::InputSlotState::ControllerLeftTrigger);
			ProcessControllerButton(xinput_state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD, platform::InputSlotState::ControllerRightTrigger);

			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP, platform::InputSlotState::ControllerDpadUp);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN, platform::InputSlotState::ControllerDpadDown);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT, platform::InputSlotState::ControllerDpadLeft);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT, platform::InputSlotState::ControllerDpadRight);
																  
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_START, platform::InputSlotState::ControllerStart);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK, platform::InputSlotState::ControllerBack);
																  
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB, platform::InputSlotState::ControllerLeftThumb);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB, platform::InputSlotState::ControllerRightThumb);
																  
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER, platform::InputSlotState::ControllerLeftShoulder);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER, platform::InputSlotState::ControllerRightShoulder);
																  
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_A, platform::InputSlotState::ControllerButtonA);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_B, platform::InputSlotState::ControllerButtonB);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_X, platform::InputSlotState::ControllerButtonX);
			ProcessControllerButton(xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y, platform::InputSlotState::ControllerButtonY);
		}
	}

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
				if (g_Platform->m_device)
				{
					display::ChangeWindowSize(g_Platform->m_device, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
				}
				//Call the game
				game->OnSizeChange(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
			}
			return 0;
		
		case WM_INPUT:
		{
			RAWINPUT input_event;
			UINT size = sizeof(input_event);

			HRAWINPUT raw_input = (HRAWINPUT)lParam;
			GetRawInputData(raw_input, RID_INPUT, &input_event, &size, sizeof(RAWINPUTHEADER));

			ProcessInputEvent(input_event);
			break;
		}
		case WM_SYSKEYDOWN:
			// Handle ALT+ENTER:
			if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
			{
				if (game && g_Platform->m_device && display::IsTearingEnabled(g_Platform->m_device))
				{
					if (g_Platform->m_windowed)
					{
						core::LogInfo("Windows is going to full screen");
						//Change to full screen

						// Save the old window rect so we can restore it when exiting fullscreen mode.
						GetWindowRect(hWnd, &g_Platform->m_window_rect);

						// Make the window borderless so that the client area can fill the screen.
						SetWindowLong(hWnd, GWL_STYLE, g_Platform->m_window_style & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

						//Get current display size
						RECT fullscreenWindowRect;
						display::Rect rect;
						if (display::GetCurrentDisplayRect(g_Platform->m_device, rect))
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

						g_Platform->m_windowed = false;
					}
					else
					{
						core::LogInfo("Windows is restoring size");

						// Restore the window's attributes and size.
						SetWindowLong(hWnd, GWL_STYLE, g_Platform->m_window_style);

						SetWindowPos(
							hWnd,
							HWND_NOTOPMOST,
							g_Platform->m_window_rect.left,
							g_Platform->m_window_rect.top,
							g_Platform->m_window_rect.right - g_Platform->m_window_rect.left,
							g_Platform->m_window_rect.bottom - g_Platform->m_window_rect.top,
							SWP_FRAMECHANGED | SWP_NOACTIVATE);

						ShowWindow(hWnd, SW_NORMAL);

						g_Platform->m_windowed = true;
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

	void RenderImgui(platform::Game* game)
	{
		if (g_Platform->m_imgui_menu_enable)
		{
			//Display menu
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("Cute"))
				{
					if (ImGui::MenuItem("Logger")) {};
					ImGui::Checkbox("Show FPS", &g_Platform->m_imgui_fps_enable);
					ImGui::Checkbox("Show Log", &g_Platform->m_imgui_log_enable);
					ImGui::Checkbox("Show Imgui Demo", &g_Platform->m_imgui_demo_enable);
					ImGui::Checkbox("Display Stats", &g_Platform->m_imgui_display_stats);
					ImGui::EndMenu();
				}
				//Call game to add it owns menus
				game->OnAddImguiMenu();

				ImGui::EndMainMenuBar();
			}
		}

		if (g_Platform->m_imgui_demo_enable)
		{
			//Show Imgui demo
			ImGui::ShowDemoWindow(&g_Platform->m_imgui_demo_enable);
		}

		if (g_Platform->m_imgui_fps_enable)
		{
			RenderFPSOverlay(g_Platform->m_last_elapsed_time);
		}

		if (g_Platform->m_imgui_display_stats && g_Platform->m_device)
		{
			display::DisplayImguiStats(g_Platform->m_device, &g_Platform->m_imgui_display_stats);
		}

		if (g_Platform->m_imgui_log_enable)
		{
			g_Platform->m_imgui_log_enable = core::LogRender();
		}

		//Render game imgui
		game->OnImguiRender();
	}
}

namespace platform
{
	HWND GetHwnd()
	{
		return g_Platform->m_current_hwnd;
	}

	//Called from the device before present with the present command list
	void PresentCallback(display::Context* context)
	{
		imgui_render::Draw(context, &g_Platform->m_imgui_draw_data[g_Platform->m_render_frame_index % Platform::kNumImguiFrames].draw_data);
	}

	void Game::SetDevice(display::Device * device)
	{
		g_Platform->m_device = device;

		//Create IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		//Init imgui
		imgui_render::Init(g_Platform->m_current_hwnd);

		//Create all resources for imgui
		imgui_render::CreateResources(device);
	}

	void Game::CaptureInput()
	{
		::CaptureInput();
	}

	bool Game::GetInputSlotState(InputSlotState input_slot) const
	{
		return g_Platform->m_input_slot_state[static_cast<size_t>(input_slot)];
	}

	float Game::GetInputSlotValue(InputSlotValue input_slot) const
	{
		return g_Platform->m_input_slot_values[static_cast<size_t>(input_slot)];
	}

	const std::vector<InputEvent> Game::GetInputEvents() const
	{
		return g_Platform->m_input_events;
	}

	bool Game::IsFocus() const
	{
		return GetForegroundWindow() == g_Platform->m_current_hwnd;
	}

	void Game::Present()
	{
		{
			PROFILE_SCOPE("Platform", "Present", 0xFFFF00FF);
			//Present
			display::Present(g_Platform->m_device);
		}

		//Increment frame index for render
		g_Platform->m_render_frame_index++;
	}

	char Run(const char* name, void* param, uint32_t width, uint32_t height, Game* game)
	{
#ifdef _STRING_HASH_MAP_ENABLED_
		core::CreateStringHashMap();
#endif

		g_Platform = new Platform;

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
		g_Platform->m_current_hwnd = CreateWindow(
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

		ShowWindow(g_Platform->m_current_hwnd, true);

		//Init input
		InitInput();

		//Init profiler system
		core::InitProfiler();
		
		//Init callback
		game->OnInit();

		//Calculate frecuency for timer
		QueryPerformanceFrequency(&g_Platform->m_frequency);
		QueryPerformanceCounter(&g_Platform->m_current_time);
		QueryPerformanceCounter(&g_Platform->m_begin_time);

		// Main sample loop.
		MSG msg = {};
		do 
		{
			//Init input
			InputFrameInit();

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
			LARGE_INTEGER last_time = g_Platform->m_current_time;
			QueryPerformanceCounter(&g_Platform->m_current_time);
			float elapsed_time = static_cast<float>(g_Platform->m_current_time.QuadPart - last_time.QuadPart) / g_Platform->m_frequency.QuadPart;
			if (elapsed_time > 0.5f)
			{
				core::LogInfo("Timestep was really high (Debugging?), limited to 30fps");
				elapsed_time = 1.f / 30.f;
			}
			g_Platform->m_total_time += elapsed_time;
			g_Platform->m_last_elapsed_time = elapsed_time;

			{
				//Capture Controller input and check if it has been more input events
				CaptureInput();
			}

			//New frame for imgui
			imgui_render::NextFrame(g_Platform->m_current_hwnd, elapsed_time);

			{
				PROFILE_SCOPE("Platform", "GameTick", 0xFFFF00FF);
				//Render
				game->OnTick(g_Platform->m_total_time, elapsed_time);
			}

			{
				PROFILE_SCOPE("Platform", "RenderPlatformImgui", 0xFFFF00FF);
				//Render platform imgui (menu, fps,...)
				RenderImgui(game);
			}

			{
				PROFILE_SCOPE("Platform", "BuildImguiRender", 0xFFFF00FF);
				//Render IMGUI
				ImGui::Render();

				//Copy the current draw data to the correct frame data
				g_Platform->m_imgui_draw_data[g_Platform->m_update_frame_index % Platform::kNumImguiFrames].Capture();
			}


			//Flip profiler
			core::FlipProfiler();

			//Increment frame index for update
			g_Platform->m_update_frame_index++;

		} while (true);

		core::LogInfo("Closing game");

		//Sync jobs preparing to destroy
		game->OnPrepareDestroy();

		//Destroy Imgui
		ImGui::DestroyContext();

		//Destroy all imgui resources
		imgui_render::DestroyResources(g_Platform->m_device);

		//Destroy callback
		game->OnDestroy();


#ifdef _STRING_HASH_MAP_ENABLED_
		core::DestroyStringHashMap();
#endif

		core::ShutdownProfiler();

		delete g_Platform;
		g_Platform = nullptr;

		// Return this part of the WM_QUIT message to Windows.
		return static_cast<char>(msg.wParam);
	}
}