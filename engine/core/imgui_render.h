//////////////////////////////////////////////////////////////////////////
// Cute engine - Imgui render using display and be call from framework
//////////////////////////////////////////////////////////////////////////

#ifndef IMGUI_RENDER_H_
#define IMGUI_RENDER_H_

#include "display/display.h"

struct ImDrawData;

namespace imgui_render
{
	//Init
	void Init(HWND hwnd);

	//Create resources
	void CreateResources(display::Device* device);

	//Destroy resources
	void DestroyResources(display::Device* device);

	//Next frame
	void NextFrame(HWND hWnd, float elapsed_time);

	//Windows message handle
	bool WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	//Draw imgui system
	void Draw(display::Context* context, ImDrawData* draw_data);
}

#endif //IMGUI_RENDER_H_