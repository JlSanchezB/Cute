#include <core/platform.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <chrono>

namespace
{
	//Windows message handle

	LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
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
			game->OnSizeChange(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
		}
			return 0;
		}

		// Handle any messages the switch statement didn't.
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	//global variable with the current hwnd, needed for specicif win32 and dx12 init
	HWND g_current_hwnd;

	//Frecuency for the perfomance timer
	LARGE_INTEGER g_frequency;
}

namespace platform
{
	HWND GetHwnd()
	{
		return g_current_hwnd;
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

			//Render
			game->OnTick();

		} while (true);

		//Destroy callback
		game->OnDestroy();

		// Return this part of the WM_QUIT message to Windows.
		return static_cast<char>(msg.wParam);
	}
}