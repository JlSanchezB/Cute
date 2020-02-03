#define NOMINMAX
#include <core/platform.h>
#include <display/display.h>
#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include <ecs/entity_component_system.h>
#include <ecs/zone_bitmask_helper.h>
#include <ext/glm/vec4.hpp>
#include <ext/glm/vec2.hpp>
#include <ext/glm/gtc/constants.hpp>
#include <core/profile.h>
#include <job/job.h>
#include <job/job_helper.h>
#include <ecs/entity_component_job_helper.h>
#include <render/render_passes_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>
#include <random>
#include <bitset>
#include <algorithm>

#include "resources.h"

class Camera
{
public:
	//Process input and update the position
	void Update(platform::Game* game, float ellapsed)
	{
		//Apply damp
		m_move_speed *= (m_damp_factor * ellapsed);
		m_rotation_speed *= (m_damp_factor * ellapsed);

		//Calculate position movement
		glm::vec3 move_speed;

		move_speed.x = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightX) * m_move_factor * ellapsed;
		move_speed.z = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightY) * m_move_factor * ellapsed;

		m_move_speed += (m_rotation * move_speed);

		//Calculate direction movement
		glm::vec3 euler_angles;
		euler_angles.y = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftY) * m_move_factor * ellapsed;
		euler_angles.z = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftX) * m_move_factor * ellapsed;

		glm::quat rotation_speed(euler_angles);

		m_rotation_speed += rotation_speed;

		//Apply
		m_position += m_move_speed * ellapsed;
		m_rotation += m_rotation_speed * ellapsed;
	}


private:

	//Position
	glm::vec3 m_position;
	glm::quat m_rotation;
	glm::vec3 m_move_speed;
	glm::quat m_rotation_speed;


	//Setup
	float m_damp_factor = 0.1f;
	float m_move_factor = 0.1f;
	float m_rotation_factor = 0.1f;
	float m_fov;
	float m_aspect_ratio;
};

//Components
struct AABoundingBox
{
	glm::vec3 min;
	glm::vec3 max;
};

struct Box
{
	//Local matrix, center of the city
	glm::mat3x4 local_matrix;
	//Dimensions -X to X, -Y to Y, -Z to Z
	glm::vec3 dimensions;
};

struct BoxRender
{
	//Index in the material array
	uint32_t material;
	//Access to gpu buffer memory
	render::AllocHandle gpu_memory;
};

//ECS definition
using BoxType = ecs::EntityType<Box, BoxRender, AABoundingBox>;

using GameComponents = ecs::ComponentList<Box, BoxRender, AABoundingBox>;
using GameEntityTypes = ecs::EntityTypeList<BoxType>;

using GameDatabase = ecs::DatabaseDeclaration<GameComponents, GameEntityTypes>;
using Instance = ecs::Instance<GameDatabase>;

//Render pass definition for our custon box instance pass render
class DrawCityBoxItemsPass : public render::Pass
{
	uint8_t m_priority;
public:
	DECLARE_RENDER_CLASS("DrawCityBoxItems");

	void Load(render::LoadContext& load_context) override
	{
		const char* value;
		if (load_context.current_xml_element->QueryStringAttribute("priority", &value) == tinyxml2::XML_SUCCESS)
		{
			m_priority = GetRenderItemPriority(load_context.render_system, render::PriorityName(value));
		}
		else
		{
			AddError(load_context, "Attribute priority expected inside DrawCityBoxItems pass");
		}
	}
	void Render(render::RenderContext& render_context) const override
	{
		//Collect all render items in the render
	}
};

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


	void OnInit() override
	{
		display::DeviceInitParams device_init_params;

#ifdef _DEBUG
		device_init_params.debug = true;
#else
		device_init_params.debug = false;
#endif

		device_init_params.width = kInitWidth;
		device_init_params.height = kInitHeight;
		device_init_params.tearing = true;
		device_init_params.vsync = false;
		device_init_params.num_frames = 3;

		m_device = display::CreateDevice(device_init_params);

		if (m_device == nullptr)
		{
			throw std::runtime_error::exception("Error creating the display device");
		}

		SetDevice(m_device);

		m_display_resources.Load(m_device);

		//Create job system
		job::SystemDesc job_system_desc;
		m_job_system = job::CreateSystem(job_system_desc);
		
		//Create Job allocator now that the job system is enabled
		m_update_job_allocator = std::make_unique<job::JobAllocator<1024 * 1024>>();

		//Create render pass system
		render::SystemDesc render_system_desc;
		m_render_system = render::CreateRenderSystem(m_device, m_job_system, this, render_system_desc);

		//Register custom passes for box city renderer
		render::RegisterPassFactory<DrawCityBoxItemsPass>(m_render_system);

		m_render_passes_loader.Load("box_city_render_passes.xml", m_render_system, m_device);

		//Create ecs database
		ecs::DatabaseDesc database_desc;
		database_desc.num_max_entities_zone = 1024;
		database_desc.num_zones = 1;
		ecs::CreateDatabase<GameDatabase>(database_desc);
	}

	void OnPrepareDestroy() override
	{
		//Sync the render and the jobs, so we can safe destroy the resources
		if (m_render_system)
		{
			render::DestroyRenderSystem(m_render_system, m_device);
		}

		if (m_job_system)
		{
			job::DestroySystem(m_job_system);
		}
	}

	void OnDestroy() override
	{
		//Destroy handles
		m_display_resources.Unload(m_device);

		//Destroy device
		display::DestroyDevice(m_device);
	}

	void OnTick(double total_time, float elapsed_time) override
	{
		//UPDATE GAME
		
	}

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override
	{
		m_width = width;
		m_height = height;
	}

	void OnAddImguiMenu() override
	{
		//Add menu for modifying the render system descriptor file
		if (ImGui::BeginMenu("ECS"))
		{
			m_render_passes_loader.GetShowEditDescriptorFile() = ImGui::MenuItem("Edit descriptor file");
			bool single_frame_mode = job::GetSingleThreadMode(m_job_system);
			if (ImGui::Checkbox("Single thread mode", &single_frame_mode))
			{
				job::SetSingleThreadMode(m_job_system, single_frame_mode);
			}
			ImGui::EndMenu();
		}
	}

	void OnImguiRender() override
	{
		m_render_passes_loader.RenderImgui();
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	BoxCityGame box_city_game;

	return platform::Run("Box city Test", param, BoxCityGame::kInitWidth, BoxCityGame::kInitHeight, &box_city_game);
}