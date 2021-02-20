#define NOMINMAX
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_ENABLE_EXPERIMENTAL

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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>
#include <random>
#include <bitset>
#include <algorithm>

#include "resources.h"

struct ViewConstantBuffer
{
	glm::mat4x4 projection_view_matrix;
	glm::vec4 time;
};

//Components
struct AABBBox
{
	glm::vec3 min;
	glm::vec3 max;
};

struct OBBBox
{
	glm::vec3 position;
	glm::mat3x3 rotation;
	glm::vec3 extents;
};

struct AnimationBox
{
	glm::vec3 original_position;
	float range; //Distance to navigate in axis Z
	float offset; //Start offset
	float frecuency; //Speed
};

struct BoxGPUHandle
{
	//Access to gpu buffer memory
	render::AllocHandle gpu_memory;
};

struct BoxRender
{
	//Index in the material array
	uint32_t material;
};

//GPU memory structs
struct GPUBoxInstance
{
	glm::vec4 local_matrix[3];

	void Fill(const OBBBox& obb_box)
	{
		glm::mat3x3 scale_rot = glm::mat3x3(glm::scale(glm::identity<glm::mat4x4>(), obb_box.extents)) * obb_box.rotation;
		local_matrix[0] = glm::vec4(scale_rot[0], obb_box.position.x);
		local_matrix[1] = glm::vec4(scale_rot[1], obb_box.position.y);
		local_matrix[2] = glm::vec4(scale_rot[2], obb_box.position.z);
	}
};

//ECS definition
using BoxType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox>;
using AnimatedBoxType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, AnimationBox>;

using GameComponents = ecs::ComponentList<BoxRender, BoxGPUHandle, OBBBox, AABBBox, AnimationBox>;
using GameEntityTypes = ecs::EntityTypeList<BoxType, AnimatedBoxType>;

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

				display::WeakVertexBufferHandle vertex_buffers[] = { m_display_resources->m_box_vertex_position_buffer, m_display_resources->m_box_vertex_normal_buffer };
				context->SetVertexBuffers(0, 1, &m_display_resources->m_box_vertex_position_buffer);
				context->SetVertexBuffers(1, 1, &m_display_resources->m_box_vertex_normal_buffer);
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

	//Camera
	helpers::FlyCamera m_camera;

	//View constant buffer
	display::ConstantBufferHandle m_view_constant_buffer;

	//Solid render priority
	render::Priority m_box_render_priority;

	BoxCityGame()
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

		std::mt19937 random(34234234);

		std::uniform_real_distribution<float> position_range(-10.f, 10.f);
		std::uniform_real_distribution<float> angle_range(-glm::half_pi<float>(), glm::half_pi<float>());
		std::uniform_real_distribution<float> length_range(1.f, 5.f);
		std::uniform_real_distribution<float> size_range(0.5f, 1.f);

		std::uniform_real_distribution<float> range_animation_range(1.0f, 5.f);
		std::uniform_real_distribution<float> frecuency_animation_range(0.3f, 1.f);
		std::uniform_real_distribution<float> offset_animation_range(0.5f, 6.f);
		//Create boxes
		for (size_t i = 0; i < 100; ++i)
		{
			OBBBox obb_box;
			float size = size_range(random);
			obb_box.position = glm::vec3(position_range(random), position_range(random), position_range(random));
			obb_box.extents = glm::vec3(size, size, length_range(random));
			obb_box.rotation = glm::rotate(angle_range(random), glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(angle_range(random), glm::vec3(0.f, 0.f, 1.f));;

			AABBBox aabb_box;
			helpers::CalculateAABBFromOBB(aabb_box.min, aabb_box.max, obb_box.position, obb_box.rotation, obb_box.extents);

			AnimationBox animated_box;
			animated_box.frecuency = frecuency_animation_range(random);
			animated_box.offset = offset_animation_range(random);
			animated_box.range = range_animation_range(random);
			animated_box.original_position = obb_box.position;

			//GPU memory
			GPUBoxInstance gpu_box_instance;
			gpu_box_instance.Fill(obb_box);

			//Allocate the GPU memory
			render::AllocHandle gpu_memory = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, sizeof(GPUBoxInstance), &gpu_box_instance, render::GetGameFrameIndex(m_render_system));

			ecs::AllocInstance<GameDatabase, AnimatedBoxType>(0)
				.Init<OBBBox>(obb_box)
				.Init<AABBBox>(aabb_box)
				.Init<AnimationBox>(animated_box)
				.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) });
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
		ecs::Process<GameDatabase, OBBBox, AABBBox, AnimationBox, BoxGPUHandle>([&](const auto& instance_iterator, OBBBox& obb_box, AABBBox& aabb_box, const AnimationBox& animation_box, const BoxGPUHandle& box_gpu_handle)
		{
				//Update position
				obb_box.position = animation_box.original_position + glm::row(obb_box.rotation, 2) * animation_box.range * static_cast<float> (cos(total_time * animation_box.frecuency + animation_box.offset));

				GPUBoxInstance gpu_box_instance;
				gpu_box_instance.Fill(obb_box);

				helpers::CalculateAABBFromOBB(aabb_box.min, aabb_box.max, obb_box.position, obb_box.rotation, obb_box.extents);

				//Allocate the GPU memory
				m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, box_gpu_handle.gpu_memory, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system));
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
		ecs::Process<GameDatabase, AABBBox, const BoxGPUHandle>([&](const auto& instance_iterator, AABBBox& aabb_box, const BoxGPUHandle& box_gpu_handle)
			{
				//Calculate if it is in the camera
				if (helpers::CollisionFrustumVsAABB(m_camera, aabb_box.min, aabb_box.max))
				{
					//Calculate sort key

					//Add this point of view
					point_of_view.PushRenderItem(m_box_render_priority, static_cast<render::SortKey>(0), static_cast<uint32_t>(m_GPU_memory_render_module->GetStaticGPUMemoryOffset(box_gpu_handle.gpu_memory)));
				}

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