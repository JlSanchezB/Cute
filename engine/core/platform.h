//////////////////////////////////////////////////////////////////////////
// Cute engine - Platform abstraction layer
//////////////////////////////////////////////////////////////////////////

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <ext/imgui/imgui.h>
#include <stdint.h>
#include <vector>
#include <helpers/interpolated.h>
#include <core/string_hash.h>

namespace display
{
	struct Device;
}

namespace render
{
	struct System;
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
		ControllerButtonA,
		ControllerButtonB,
		ControllerButtonX,
		ControllerButtonY,
		ControllerDpadUp,
		ControllerDpadDown,
		ControllerDpadLeft,
		ControllerDpadRight,
		ControllerStart,
		ControllerBack,
		ControllerLeftThumb,
		ControllerRightThumb,
		ControllerLeftShoulder,
		ControllerRightShoulder,
		ControllerLeftTrigger,
		ControllerRightTrigger,
		Key_A,
		Key_B,
		Key_C,
		Key_D,
		Key_E,
		Key_F,
		Key_G,
		Key_H,
		Key_I,
		Key_J,
		Key_L,
		Key_M,
		Key_N,
		Key_O,
		Key_P,
		Key_Q,
		Key_R,
		Key_S,
		Key_T,
		Key_U,
		Key_V,
		Key_W,
		Key_X,
		Key_Y,
		Key_Z,
		Key_1,
		Key_2,
		Key_3,
		Key_4,
		Key_5,
		Key_6,
		Key_7,
		Key_8,
		Key_9,
		Key_0,
		Count,
		Invalid = 255
	};

	enum class InputSlotValue : uint8_t
	{
		MousePositionX,
		MousePositionY,
		MouseRelativePositionX,
		MouseRelativePositionY,
		ControllerLeftTrigger,
		ControllerRightTrigger,
		ControllerThumbLeftX,
		ControllerThumbLeftY,
		ControllerThumbRightX,
		ControllerThumbRightY,
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

	struct FrameInterpolationControl
	{
		inline static size_t s_frame = 0; 
		inline static float s_interpolation_value = 0.f;
		inline static bool s_interpolate_phase = false;
		inline static bool s_update_phase = false;
	};

	//Interpolate wraper to use with the platform LogicRender update type
	template<class DATA>
	using Interpolated = helpers::Interpolated<DATA, FrameInterpolationControl>;

	enum class UpdateType
	{
		Tick, //Each frame just call a OnTick
		LogicRender //Each frame calls to OnLogic in fixed FPS and OnRender as much as it needs
	};

	using ImguiDebugSystemName = StringHash32<"ImguiDebugSystemName"_namespace>;

	//Virtual interface that implements a game
	class Game
	{
	protected:
		void SetDevice(display::Device* device);
		void SetRenderSystem(render::System* render_system);
		void SetUpdateType(UpdateType update_type, float fixed_logic_framerate);
	public:
		
		//Input
		void CaptureInput(); //Recapture the input in the midle of the frame if needed
		bool GetInputSlotState(InputSlotState input_slot) const;
		float GetInputSlotValue(InputSlotValue input_slot) const;
		const std::vector<InputEvent> GetInputEvents() const;

		// Is the window focus
		bool IsWindowFocus() const;
		// It is the game focus, it can be false when IMGUI is enable
		bool IsFocus() const;

		//Capture/Release mouse
		void CaptureMouse();
		void ReleaseMouse();

		//Show Cursor
		void ShowCursor(bool show);

		//Imgui debug systems
		void RegisterImguiDebugSystem(const ImguiDebugSystemName& name, std::function<void(bool*)>&& function);

		//Present 
		//Needs to be called at the end of each tick
		//or from the render thread (if any)
		void Present();

		//Interface
		virtual void OnInit() = 0;
		virtual void OnPrepareDestroy() {}; //In case a job needs to be sync
		virtual void OnDestroy() = 0;
		virtual void OnTick(double total_time, float elapsed_time) {};
		virtual void OnLogic(double total_time, float elapsed_time) {};
		virtual void OnRender(double total_time, float elapsed_time) {};
		virtual void OnSizeChange(uint32_t width, uint32_t height, bool minimized) = 0;
		virtual void OnAddImguiMenu() {};
		virtual void OnImguiRender() {};
	};

	char Run(const char* name, void* param, uint32_t width, uint32_t height, Game* game);

	//Show modal dialog, returns if yes/no was clicked
	bool ShowModalDialog(const char* title, const char* message);

	//Platform module
	struct Module
	{
		virtual ~Module() {};
		virtual void OnInit(display::Device* device, render::System* render_system) {};
		virtual void OnPrepareDestroy() {}; //In case a job needs to be sync
		virtual void OnDestroy() {};
		virtual void OnResetFrame() {};
		virtual void OnTick(double total_time, float elapsed_time) {};
		virtual void OnLogic(double total_time, float elapsed_time) {};
		virtual void OnRender(double total_time, float elapsed_time) {};
	};

	//Register a module for the platform, usually happens during global init
	void RegisterModule(Module* module);
}

#endif PLATFORM_H_