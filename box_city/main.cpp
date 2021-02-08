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
#include <render_module/render_module_gpu_memory.h>

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
	void Update(platform::Game* game, float ellapsed_time)
	{
		//Apply damp
		m_move_speed *= (m_damp_factor * ellapsed_time);
		m_rotation_speed *= (m_damp_factor * ellapsed_time);

		//Calculate position movement
		glm::vec3 move_speed;

		move_speed.x = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightX) * m_move_factor * ellapsed_time;
		move_speed.z = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightY) * m_move_factor * ellapsed_time;

		m_move_speed += (m_rotation * move_speed);

		//Calculate direction movement
		glm::vec3 euler_angles;
		euler_angles.y = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftY) * m_move_factor * ellapsed_time;
		euler_angles.z = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftX) * m_move_factor * ellapsed_time;

		glm::quat rotation_speed(euler_angles);

		m_rotation_speed += rotation_speed;

		//Apply
		m_position += m_move_speed * ellapsed_time;
		m_rotation += m_rotation_speed * ellapsed_time;

		CalculateMatrices();
	}

	void CalculateMatrices()
	{
		//Calculate view to world
		glm::mat3x3 rot(m_rotation);
		m_view_to_world_matrix = glm::lookAtRH(rot[1], m_position, m_up_vector);

		//Calculate world to view
		m_world_to_view_matrix = glm::inverse(m_view_to_world_matrix);

		//Calculate projection matrix
		m_projection_matrix = glm::perspective(m_fov_y, m_aspect_ratio, m_near, m_far);

		//Calculate view projection
		m_view_projection_matrix = m_projection_matrix * m_world_to_view_matrix;
	}

	glm::mat4x4 GetViewProjectionMatrix() const
	{
		return m_view_projection_matrix;
	}

private:

	//State
	glm::vec3 m_position = glm::vec3(0.f, 0.f, 0.f);
	glm::quat m_rotation = glm::quat();
	glm::vec3 m_move_speed = glm::vec3(0.f, 0.f, 0.f);
	glm::quat m_rotation_speed = glm::quat();
	glm::vec3 m_up_vector = glm::vec3(0.f, 0.f, 1.f);

	//Matrices
	glm::mat4x4 m_view_to_world_matrix;
	glm::mat4x4 m_world_to_view_matrix;
	glm::mat4x4 m_projection_matrix;
	glm::mat4x4 m_view_projection_matrix;


	//Setup
	float m_damp_factor = 0.1f;
	float m_move_factor = 0.1f;
	float m_rotation_factor = 0.1f;
	float m_fov_y = 1.f;
	float m_aspect_ratio = 1.f;
	float m_far = 10000.f;
	float m_near = 0.1f;
};

struct ViewConstantBuffer
{
	glm::mat4x4 projection_view_matrix;
	glm::vec4 time;
};

//GPU memory structs
struct GPUBoxInstance
{
	glm::vec4 bounding_box_min;
	glm::vec4 bounding_box_max;
	glm::mat4x3 local_matrix;
	glm::vec4 dimensions;
};

//Components
struct AABoundingBox
{
	glm::vec3 min;
	glm::vec3 max;
};

struct Box
{
	//Random id 
	uint32_t id;
	//Local matrix, center of the city
	glm::mat4x4 local_matrix;
	//Dimensions -X to X, -Y to Y, -Z to Z
	glm::vec3 dimensions;

	Box(const uint32_t _id, const glm::mat4x4& _local_matrix, const glm::vec3& _dimensions):
		id(_id), local_matrix(_local_matrix), dimensions(_dimensions)
	{
	}
};

struct BoxRender
{
	//Index in the material array
	uint32_t material;
	//Access to gpu buffer memory
	render::AllocHandle gpu_memory;

	BoxRender(uint32_t _material, render::AllocHandle& _gpu_memory)
		: material(_material), gpu_memory(std::move(_gpu_memory))
	{
	}
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

	//GPU Memory render module
	render::GPUMemoryRenderModule* m_GPU_memory_render_module = nullptr;

	//Random generators
	std::random_device m_random_device;
	std::mt19937 m_random_generator;

	std::uniform_real_distribution<float> m_random_position_x;
	std::uniform_real_distribution<float> m_random_position_y;
	std::uniform_real_distribution<float> m_random_position_z;

	//Camera
	Camera m_camera;

	//View constant buffer
	display::ConstantBufferHandle m_view_constant_buffer;

	//Solid render priority
	render::Priority m_box_render_priority;

	BoxCityGame() : m_random_generator(m_random_device()),
		m_random_position_x(-100.f, 100.f),
		m_random_position_y(-100.f, 100.f),
		m_random_position_z(-100.f, 100.f)
	{
	}

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

		//Create view constant buffer
		display::ConstantBufferDesc view_constant_desc;
		view_constant_desc.size = sizeof(ViewConstantBuffer);
		view_constant_desc.access = display::Access::Dynamic;
		m_view_constant_buffer = display::CreateConstantBuffer(m_device, view_constant_desc, "ViewConstantBuffer");

		//Create job system
		job::SystemDesc job_system_desc;
		m_job_system = job::CreateSystem(job_system_desc);
		
		//Create Job allocator now that the job system is enabled
		m_update_job_allocator = std::make_unique<job::JobAllocator<1024 * 1024>>();

		//Create render pass system
		render::SystemDesc render_system_desc;
		m_render_system = render::CreateRenderSystem(m_device, m_job_system, this, render_system_desc);

		//Register gpu memory render module
		render::GPUMemoryRenderModule::GPUMemoryDesc gpu_memory_desc;
		//gpu_memory_desc.dynamic_gpu_memory_size = 512 * 1024;
		//gpu_memory_desc.dynamic_gpu_memory_segment_size = 32 * 1024;

		m_GPU_memory_render_module = render::RegisterModule<render::GPUMemoryRenderModule>(m_render_system, "GPUMemory"_sh32, gpu_memory_desc);

		//Register custom passes for box city renderer
		render::RegisterPassFactory<DrawCityBoxItemsPass>(m_render_system);

		render::AddGameResource(m_render_system, "BackBuffer"_sh32, CreateResourceFromHandle<render::RenderTargetResource>(display::GetBackBuffer(m_device), m_width, m_height));

		//Register the ViewConstantBuffer for Main pass, ID 0
		render::AddGameResource(m_render_system, "ViewConstantBuffer"_sh32, "Main"_sh32, 0, CreateResourceFromHandle<render::ConstantBufferResource>(display::WeakConstantBufferHandle(m_view_constant_buffer)));

		m_render_passes_loader.Load("box_city_render_passes.xml", m_render_system, m_device);

		//Get render priorities
		m_box_render_priority = render::GetRenderItemPriority(m_render_system, "Box"_sh32);

		//Create ecs database
		ecs::DatabaseDesc database_desc;
		database_desc.num_max_entities_zone = 1024;
		database_desc.num_zones = 1;
		ecs::CreateDatabase<GameDatabase>(database_desc);

		//Create boxes
		for (size_t i = 0; i < 100; ++i)
		{
			uint32_t id = m_random_generator();
			float position_z = static_cast<float>(id - m_random_generator.min()) / static_cast<float>(m_random_generator.max() - m_random_generator.min());
			glm::mat4x4 local_matrix;
			glm::translate(local_matrix, glm::vec3(m_random_position_x(m_random_generator), m_random_position_y(m_random_generator), position_z));
			glm::vec3 dimensions(1.f, 1.f, 1.f);
			
			//GPU memory
			GPUBoxInstance gpu_box_instance;
			gpu_box_instance.local_matrix = local_matrix;
			gpu_box_instance.dimensions = glm::vec4(dimensions, 0.f);

			//Allocate the GPU memory
			render::AllocHandle gpu_memory = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, sizeof(GPUBoxInstance), &gpu_box_instance, render::GetGameFrameIndex(m_render_system));

			ecs::AllocInstance<GameDatabase, BoxType>(0)
				.Init<Box>(id, local_matrix, dimensions)
				.Init<BoxRender>(0, gpu_memory);
		}
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
		//Destroy constant buffer
		display::DestroyHandle(m_device, m_view_constant_buffer);

		//Destroy handles
		m_display_resources.Unload(m_device);

		//Destroy device
		display::DestroyDevice(m_device);
	}

	void OnTick(double total_time, float elapsed_time) override
	{
		//UPDATE GAME

		//Update camera
		m_camera.Update(this, elapsed_time);

		render::BeginPrepareRender(m_render_system);

		//Update all positions for testing the static gpu memory
		ecs::Process<GameDatabase, Box, const BoxRender>([&](const auto& instance_iterator, Box& box, const BoxRender& box_render)
		{
				//Update local position
				float position_z = static_cast<float>(cos(total_time)) + static_cast<float>(box.id - m_random_generator.min()) / static_cast<float>(m_random_generator.max() - m_random_generator.min());
				box.local_matrix[3][2] = position_z;

				GPUBoxInstance gpu_box_instance;
				gpu_box_instance.local_matrix = box.local_matrix;
				gpu_box_instance.dimensions = glm::vec4(box.dimensions, 0.f);

				//Allocate the GPU memory
				m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, box_render.gpu_memory, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system));
		}, std::bitset<1>(true));
			
		//Check if the render passes loader needs to load
		m_render_passes_loader.Update();
			
		render::PassInfo pass_info;
		pass_info.Init(m_width, m_height);

		render::Frame& render_frame = render::GetGameRenderFrame(m_render_system);

		//Update view constant buffer with the camera and the time
		auto command_offset = render_frame.GetBeginFrameCommandBuffer().Open();
		ViewConstantBuffer view_constant_buffer;
		view_constant_buffer.projection_view_matrix = m_camera.GetViewProjectionMatrix();
		view_constant_buffer.time = glm::vec4(static_cast<float>(total_time), 0.f, 0.f, 0.f);
		render_frame.GetBeginFrameCommandBuffer().UploadResourceBuffer(m_view_constant_buffer, &view_constant_buffer, sizeof(view_constant_buffer));
		render_frame.GetBeginFrameCommandBuffer().Close();

		//Add render passes
		render_frame.AddRenderPass("Main"_sh32, 0, pass_info, "Main"_sh32, 0);
		render_frame.AddRenderPass("SyncStaticGPUMemory"_sh32, 0, pass_info);
		
		//Add point of view
		auto& point_of_view = render_frame.AllocPointOfView("Main"_sh32, 0);
		
		//Cull box city
		ecs::Process<GameDatabase, Box, const BoxRender>([&](const auto& instance_iterator, Box& box, const BoxRender& box_render)
			{
				//Calculate if it is in the camera

				//Calculate sort key

				//Add this point of view
				point_of_view.PushRenderItem(m_box_render_priority, static_cast<render::SortKey>(0), {});

			}, std::bitset<1>(true));

		//Render
		render::EndPrepareRenderAndSubmit(m_render_system);

		{
			PROFILE_SCOPE("ECSTest", "DatabaseTick", 0xFFFF77FF);
			//Tick database
			ecs::Tick<GameDatabase>();
		}
	}

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override
	{
		m_width = width;
		m_height = height;

		if (m_render_system)
		{
			render::GetResource<render::RenderTargetResource>(m_render_system, "BackBuffer"_sh32)->UpdateInfo(width, height);
		}
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