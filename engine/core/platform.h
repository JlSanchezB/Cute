//////////////////////////////////////////////////////////////////////////
// Cute engine - Platform abstraction layer
//////////////////////////////////////////////////////////////////////////

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <core/imgui/imgui.h>

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
		virtual void OnSizeChange(size_t width, size_t height, bool minimized) = 0;
	};

	char Run(const char* name, void* param, size_t width, size_t height, Game* game);
}

#endif PLATFORM_H_