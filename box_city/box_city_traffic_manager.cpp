#include "box_city_traffic_manager.h"

namespace BoxCityTrafficSystem
{
	void Manager::Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module)
	{
		m_device = device;
		m_render_system = render_system;
		m_GPU_memory_render_module = GPU_memory_render_module;
	}

	void Manager::Shutdown()
	{

	}

	void Manager::Update()
	{

	}
}