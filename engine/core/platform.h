//////////////////////////////////////////////////////////////////////////
// Cute engine - Platform abstraction layer
//////////////////////////////////////////////////////////////////////////

#ifndef PLATFORM_H_
#define PLATFORM_H_

namespace platform
{
	//Virtual interface that implements a game
	class Game
	{
	public:
		virtual void OnInit() = 0;
		virtual void OnDestroy() = 0;
		virtual void OnTick() = 0;
	};

	char Run(const char* name, void* param, size_t width, size_t height, Game* game);
}

#endif PLATFORM_H_