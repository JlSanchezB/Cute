#define NOMINMAX
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED

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
#include <ext/glm/gtx/euler_angles.hpp>

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
		//Read inputs
		float forward_input = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightY);
		if (game->GetInputSlotState(platform::InputSlotState::Up)) forward_input += 1.f;
		if (game->GetInputSlotState(platform::InputSlotState::Down)) forward_input -= 1.f;
		float side_input = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightX);
		if (game->GetInputSlotState(platform::InputSlotState::Right)) side_input += 1.f;
		if (game->GetInputSlotState(platform::InputSlotState::Left)) side_input -= 1.f;
		float up_input = (game->GetInputSlotValue(platform::InputSlotValue::ControllerRightTrigger) - game->GetInputSlotValue(platform::InputSlotValue::ControllerLeftTrigger));
		if (game->GetInputSlotState(platform::InputSlotState::PageUp)) up_input += 1.f;
		if (game->GetInputSlotState(platform::InputSlotState::PageDown)) up_input -= 1.f;

		if (game->GetInputSlotState(platform::InputSlotState::RightMouseButton))
		{
			side_input += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * m_mouse_move_factor;
			forward_input += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * m_mouse_move_factor;
		}

		forward_input = glm::clamp(forward_input, -1.f, 1.f);
		side_input = glm::clamp(side_input, -1.f, 1.f);
		up_input = glm::clamp(up_input, -1.f, 1.f);

		glm::vec2 rotation_input;
		rotation_input.x = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftX);
		rotation_input.y = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftY);
		if (game->GetInputSlotState(platform::InputSlotState::LeftMouseButton))
		{
			rotation_input.x += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * m_mouse_rotate_factor;
			rotation_input.y -= game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * m_mouse_rotate_factor;
		}
		rotation_input.x = glm::clamp(rotation_input.x, -1.f, 1.f);
		rotation_input.y = glm::clamp(rotation_input.y, -1.f, 1.f);

		//Apply damp
		m_move_speed -= m_move_speed * glm::clamp((m_damp_factor * ellapsed_time), 0.f, 1.f);
		m_rotation_speed -= m_rotation_speed * glm::clamp((m_damp_factor * ellapsed_time), 0.f, 1.f);

		//Calculate position movement
		glm::vec3 move_speed;

		glm::mat3x3 rot = glm::rotate(m_rotation.y, glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(m_rotation.x, glm::vec3(0.f, 0.f, 1.f));
		//Define forward and side
		glm::vec3 forward = glm::vec3(0.f, 1.f, 0.f) * rot;
		glm::vec3 side = glm::vec3(1.f, 0.f, 0.f) * rot;

		move_speed = -side_input * m_move_factor * ellapsed_time * side;
		move_speed += forward_input * m_move_factor * ellapsed_time * forward;
		//Up/Down doesn't depend of the rotation
		move_speed.z += up_input * m_move_factor * ellapsed_time;

		
		m_move_speed += move_speed;

		//Calculate direction movement
		glm::vec2 angles;
		angles.x = -rotation_input.x * m_rotation_factor * ellapsed_time;
		angles.y = -rotation_input.y * m_rotation_factor * ellapsed_time;

		m_rotation_speed += angles;

		//Apply
		m_position += m_move_speed * ellapsed_time;
		m_rotation += m_rotation_speed * ellapsed_time;

		if (m_rotation.y > glm::radians(85.f)) m_rotation.y = glm::radians(85.f);
		if (m_rotation.y < glm::radians(-85.f)) m_rotation.y = glm::radians(-85.f);

		CalculateMatrices();
	}

	void CalculateMatrices()
	{
		//Calculate view to world
		glm::mat3x3 rot = glm::rotate(m_rotation.y, glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(m_rotation.x, glm::vec3(0.f, 0.f, 1.f));
		m_world_to_view_matrix = glm::lookAt(m_position, m_position + glm::vec3(0.f, 1.f, 0.f) * rot , m_up_vector);

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
	glm::vec2 m_rotation = glm::vec3(0.f);
	glm::vec3 m_move_speed = glm::vec3(0.f, 0.f, 0.f);
	glm::vec2 m_rotation_speed = glm::vec2(0.f, 0.f);
	glm::vec3 m_up_vector = glm::vec3(0.f, 0.f, 1.f);

	//Matrices
	glm::mat4x4 m_world_to_view_matrix;
	glm::mat4x4 m_projection_matrix;
	glm::mat4x4 m_view_projection_matrix;


	//Setup
	float m_mouse_rotate_factor = 2.5f;
	float m_mouse_move_factor = 5.0f;
	float m_damp_factor = 5.0f;
	float m_move_factor = 40.0f;
	float m_rotation_factor = 20.f;
	float m_fov_y = glm::radians(90.f);
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
	glm::mat4x4 local_matrix;
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
	inline static DisplayResource* m_display_resources;

	friend class BoxCityGame;
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
		//Collect all render items to render
		const render::PointOfView* point_of_view = render_context.GetPointOfView();
		if (point_of_view)
		{
			auto& sorted_render_items = point_of_view->GetSortedRenderItems();
			const size_t begin_render_item = sorted_render_items.m_priority_table[m_priority].first;
			const size_t end_render_item = sorted_render_items.m_priority_table[m_priority].second;

			if (begin_render_item < end_render_item)
			{
				//Allocate a buffer that for each box has an offset to the box data
				auto* gpu_memory = render::GetModule<render::GPUMemoryRenderModule>(render_context.GetRenderSystem(), "GPUMemory"_sh32);

				uint32_t* instances_ptrs = reinterpret_cast<uint32_t*>(gpu_memory->AllocDynamicGPUMemory(render_context.GetDevice(), (end_render_item - begin_render_item + 1) * sizeof(int32_t), render::GetRenderFrameIndex(render_context.GetRenderSystem())));

				//Upload all the instances offsets
				for (size_t render_item_index = begin_render_item; render_item_index <= end_render_item; ++render_item_index)
				{
					//Just copy the offset
					instances_ptrs[render_item_index - begin_render_item] = sorted_render_items.m_sorted_render_items[render_item_index].data;
				}

				uint32_t offset_to_instance_offsets = static_cast<uint32_t>(gpu_memory->GetDynamicGPUMemoryOffset(render_context.GetDevice(), instances_ptrs));

				auto context = render_context.GetContext();
				//Set the offset as a root constant
				context->SetConstants(display::Pipe::Graphics, 0, &offset_to_instance_offsets, 1);

				//Render
				display::WeakPipelineStateHandle box_pipeline_state = render::GetResource<render::GraphicsPipelineStateResource>(render_context.GetRenderSystem(), "BoxPipelineState"_sh32)->GetHandle();
				context->SetPipelineState(box_pipeline_state);

				context->SetVertexBuffers(0, 1, &m_display_resources->m_box_vertex_buffer);
				context->SetIndexBuffer(m_display_resources->m_box_index_buffer);

				display::DrawIndexedInstancedDesc desc;
				desc.instance_count = static_cast<uint32_t>((end_render_item - begin_render_item + 1));
				desc.index_count = 36;
				context->DrawIndexedInstanced(desc);
			}
		}
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
		m_random_position_x(-10.f, 10.f),
		m_random_position_y(-10.f, 10.f),
		m_random_position_z(-10.f, 10.f)
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
		DrawCityBoxItemsPass::m_display_resources = &m_display_resources;

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
			float position_z = 10.f * static_cast<float>(id - m_random_generator.min()) / static_cast<float>(m_random_generator.max() - m_random_generator.min());
			glm::mat4x4 local_matrix;
			local_matrix = glm::translate(glm::identity<glm::mat4x4>(), glm::vec3(m_random_position_x(m_random_generator), m_random_position_y(m_random_generator), position_z));
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
				float position_z = 10.f * static_cast<float>(cos(total_time * 0.1f + static_cast<float>(box.id) * 0.1f)) + static_cast<float>(box.id - m_random_generator.min()) / static_cast<float>(m_random_generator.max() - m_random_generator.min());
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
				point_of_view.PushRenderItem(m_box_render_priority, static_cast<render::SortKey>(0), static_cast<uint32_t>(m_GPU_memory_render_module->GetStaticGPUMemoryOffset(box_render.gpu_memory)));

			}, std::bitset<1>(true));

		//Render
		render::EndPrepareRenderAndSubmit(m_render_system);

		{
			PROFILE_SCOPE("ECSTest", 0xFFFF77FF, "DatabaseTick");
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