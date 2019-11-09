//////////////////////////////////////////////////////////////////////////
// Cute engine - Platform abstraction layer
//////////////////////////////////////////////////////////////////////////

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <ext/imgui/imgui.h>
#include <stdint.h>
#include <vector>

namespace display
{
	struct Device;
}

namespace platform
{
	enum class InputSlotState : uint8_t
	{
		Back,
		Tab,
		Return,
		LShift,
		LControl,
		RShift,
		RControl,
		Escape,
		Space,
		Left,
		Up,
		Right,
		Down,
		PageUp,
		PageDown,
		LeftMouseButton,
		RightMouseButton,
		MiddleMouseButton,

		Count,
		Invalid = 255
	};

	enum class InputSlotValue : uint8_t
	{
		MousePositionX,
		MousePositionY,

		Count,
		Invalid = 255
	};

	enum class EventType : uint8_t
	{
		KeyUp,
		KeyDown,
		MouseWheel
	};

	struct InputEvent
	{
		EventType type;
		InputSlotState slot;
		float value;

		InputEvent(EventType _type, InputSlotState _slot) : type(_type), slot(_slot)
		{
		}
		InputEvent(EventType _type, float _value) : type(_type), value(_value)
		{
		}
	};

	//Virtual interface that implements a game
	class Game
	{
	protected:
		void SetDevice(display::Device* device);

		//Input
		bool GetInputSlotState(InputSlotState input_slot) const;
		float GetInputSlotValue(InputSlotValue input_slot) const;
		const std::vector<InputEvent> GetInputEvents() const;

	public:
		//Present 
		//Needs to be called at the end of each tick
		//or from the render thread (if any)
		void Present();

		//Interface
		virtual void OnInit() = 0;
		virtual void OnPrepareDestroy() {}; //In case a job needs to be sync
		virtual void OnDestroy() = 0;
		virtual void OnTick(double total_time, float elapsed_time) = 0;
		virtual void OnSizeChange(uint32_t width, uint32_t height, bool minimized) = 0;
		virtual void OnAddImguiMenu() {};
		virtual void OnImguiRender() {};
	};

	char Run(const char* name, void* param, uint32_t width, uint32_t height, Game* game);
}

#endif PLATFORM_H_