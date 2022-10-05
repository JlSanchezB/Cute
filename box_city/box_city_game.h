#ifndef BOX_CITY_GAME_H
#define BOX_CITY_GAME_H

#include <core/platform.h>
#include <display/display.h>
#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include <ecs/entity_component_system.h>
#include <ecs/zone_bitmask_helper.h>
#include <core/profile.h>
#include <job/job.h>
#include <job/job_helper.h>
#include <ecs/entity_component_job_helper.h>
#include <render/render_passes_loader.h>
#include <render_module/render_module_gpu_memory.h>
#include <helpers/camera.h>
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <ext/glm/gtx/euler_angles.hpp>
#include <ext/glm/gtc/matrix_access.hpp>
#include <helpers/collision.h>

#include "resources.h"
#include "box_city_components.h"
#include "box_city_render.h"
#include "box_city_tile_manager.h"


class BoxCityGame : public platform::Game
{
public:
	constexpr static uint32_t kInitWidth = 500;
	constexpr static uint32_t kInitHeight = 500;

	uint32_t m_width;
	uint32_t m_height;

	display::Device* m_device;

	render::System* m_render_system = nullptr;

	job::System* m_job_system = nullptr;

	//Job allocator, it needs to be created in the onInit, that means that the job system is not created during the GameConstructor
	std::unique_ptr<job::JobAllocator<1024 * 1024>> m_update_job_allocator;

	//Display resources
	DisplayResource m_display_resources;

	//Render passes loader
	render::RenderPassesLoader m_render_passes_loader;

	//GPU Memory render module
	render::GPUMemoryRenderModule* m_GPU_memory_render_module = nullptr;

	//Camera
	helpers::FlyCamera m_camera = helpers::FlyCamera(helpers::Camera::ZRange::OneZero);

	//View constant buffer
	display::ConstantBufferHandle m_view_constant_buffer;

	//Solid render priority
	render::Priority m_box_render_priority;

	//Tile manager
	BoxCityTileSystem::Manager m_tile_manager;

	//Sun direction
	glm::vec2 m_sun_direction_angles = glm::vec2(45.f, 45.f);

	BoxCityGame()
	{
	}

	void OnInit() override;

	void OnPrepareDestroy() override;

	void OnDestroy() override;

	void OnTick(double total_time, float elapsed_time) override;

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override;

	void OnAddImguiMenu() override;

	void OnImguiRender() override;
};

#endif //BOX_CITY_GAME_H