//////////////////////////////////////////////////////////////////////////
// Cute engine - Debug primitives
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_DEBUG_PRIMITIVES_H_
#define RENDER_DEBUG_PRIMITIVES_H_

#include <ext/glm/glm.hpp>

namespace display
{
	struct Device;
}

namespace render
{
	struct System;
	class GPUMemoryRenderModule;

	namespace debug_primitives
	{
		struct Colour
		{
			union
			{
				struct
				{
					uint8_t r;
					uint8_t g;
					uint8_t b;
					uint8_t a;
				};
				uint32_t value;
			};
			constexpr Colour(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) : r(red), g(green), b(blue), a(alpha){};
		};

		constexpr Colour kRed = Colour(0xFF, 0x00, 0x00, 0xFF);
		constexpr Colour kGreen = Colour(0x00, 0xFF, 0x00, 0xFF);
		constexpr Colour kBlue = Colour(0xFF, 0x00, 0xFF, 0xFF);
		constexpr Colour kYellow = Colour(0xFF, 0xFF, 0x00, 0xFF);
		constexpr Colour kCyan = Colour(0x00, 0xFF, 0xFF, 0xFF);
		constexpr Colour kMagenta = Colour(0xFF, 0x00, 0xFF, 0xFF);
		constexpr Colour kOrange = Colour(0xFF, 0xA5, 0x00, 0xFF);
		constexpr Colour kDeepPink = Colour(0xFF, 0x14, 0x96, 0xFF);
		constexpr Colour kWhite = Colour(0xFF, 0xFF, 0xFF, 0xFF);

		//Register interface
		void Init(display::Device* device, System* system, GPUMemoryRenderModule* gpu_memory_render_module);
		void Shutdown(display::Device* device, System* system);

		//Each frame needs to know the view projection matrix
		void SetViewProjectionMatrix(const glm::mat4x4& view_projection_matrix);
		
		//Debug draw commands
		void DrawLine(const glm::vec3& position_a, const glm::vec3& position_b, const Colour& colour);
		void DrawStar(const glm::vec3& position, const float size, const Colour& colour);
	}
}

#endif //RENDER_DEBUG_PRIMITIVES_H_