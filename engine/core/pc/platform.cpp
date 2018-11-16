#include <core/platform.h>
#include <display/display.h>
#include <core/imgui/imgui.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <chrono>
#include <core/imgui_render.h>

namespace display
{
	//Size change
	void ChangeWindowSize(Device* device, size_t width, size_t height, bool minimized);

	//Only need to control the resolution from the platform layer if tearing is enabled
	bool IsTearingEnabled(Device* device);

	bool GetCurrentDisplayRect(Device* device, Rect& rect);
}

namespace
{
	//Device
	display::Device* g_device = nullptr;

	//Fullscreen
	RECT g_window_rect;
	UINT g_window_style = WS_OVERLAPPEDWINDOW;
	bool g_windowed = true;

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

				//Call the device
				if (g_device)
				{
					display::ChangeWindowSize(g_device, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
				}
				//Call the game
				game->OnSizeChange(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
			}
			return 0;

		case WM_SYSKEYDOWN:
			// Handle ALT+ENTER:
			if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
			{
				if (game && g_device && display::IsTearingEnabled(g_device))
				{
					if (g_windowed)
					{
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

	//global variable with the current hwnd, needed for specicif win32 and dx12 init
	HWND g_current_hwnd;

	//Frecuency for the perfomance timer
	LARGE_INTEGER g_frequency;

	//Frecuency for the perfomance timer
	LARGE_INTEGER g_current_time;

	//Begin time
	LARGE_INTEGER g_begin_time;
}

namespace platform
{
	HWND GetHwnd()
	{
		return g_current_hwnd;
	}

	//Called from the device before present with the present command list
	void PresentCallback(display::Device* device, display::CommandListHandle& commandlist_handle)
	{
		ImGui::Render();
		imgui_render::Draw(device, commandlist_handle);
	}

	void Game::SetDevice(display::Device * device)
	{
		g_device = device;

		//Create IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		//Create all resources for imgui
		imgui_render::CreateResources(device);
	}

	char Run(const char* name, void* param, size_t width, size_t height, Game* game)
	{
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

		//Calculate frecuency for timer
		QueryPerformanceFrequency(&g_frequency);
		QueryPerformanceCounter(&g_current_time);
		QueryPerformanceCounter(&g_begin_time);

		//Init callback
		game->OnInit();

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
			double total_time = static_cast<double>(g_current_time.QuadPart - g_begin_time.QuadPart) / g_frequency.QuadPart;
			
			//New frame for imgui
			imgui_render::NextFrame(g_current_hwnd, elapsed_time);

			//Render
			game->OnTick(total_time, elapsed_time);

		} while (true);

		//Destroy all imgui resources
		imgui_render::DestroyResources(g_device);
		//Destroy Imgui
		ImGui::DestroyContext();

		//Destroy callback
		game->OnDestroy();

		// Return this part of the WM_QUIT message to Windows.
		return static_cast<char>(msg.wParam);
	}
}