//////////////////////////////////////////////////////////////////////////
// Cute engine - Platform abstraction layer
//////////////////////////////////////////////////////////////////////////

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <ext/imgui/imgui.h>
#include <stdint.h>

namespace display
{
	struct Device;
}

namespace platform
{
	//Virtual interface that implements a game
	class Game
	{
	protected:
		void SetDevice(display::Device* device);
	public:
		//Interface
		virtual void OnInit() = 0;
		virtual void OnDestroy() = 0;
		virtual void OnTick(double total_time, float elapsed_time) = 0;
		virtual void OnSizeChange(uint32_t width, uint32_t height, bool minimized) = 0;
		virtual void OnAddImguiMenu() {};
	};

	char Run(const char* name, void* param, uint32_t width, uint32_t height, Game* game);
}

#endif PLATFORM_H_