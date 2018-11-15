//////////////////////////////////////////////////////////////////////////
// Cute engine - Imgui render using display and be call from framework
//////////////////////////////////////////////////////////////////////////

#ifndef IMGUI_RENDER_H_
#define IMGUI_RENDER_H_

#include "display/display.h"

namespace imgui_render
{
	//Create resources
	void CreateResources(display::Device* device);

	//Destroy resources
	void DestroyResources(display::Device* device);

	//Draw imgui system
	void Draw(display::Device* device, const display::CommandListHandle& command_list_handle);
}

#endif //IMGUI_RENDER_H_