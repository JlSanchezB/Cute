#ifndef BOX_CITY_TRAFFIC_MANAGER_H
#define BOX_CITY_TRAFFIC_MANAGER_H

#include "box_city_components.h"

namespace render
{
	class GPUMemoryRenderModule;
}

namespace BoxCityTrafficSystem
{
	//Number of cars in the simulation
	constexpr uint32_t kNumCars = 1000;
	//Area around the camera with the traffic simulation
	constexpr float kCameraRange = 3000.f;

	class Manager
	{
	public:
		//Init
		void Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);

		//Shutdown
		void Shutdown();

		//Update
		void Update();

	private:
		//System
		display::Device* m_device = nullptr;
		render::System* m_render_system = nullptr;
		render::GPUMemoryRenderModule* m_GPU_memory_render_module = nullptr;
	};
}

#endif //BOX_CITY_TRAFFIC_MANAGER_H