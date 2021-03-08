#include "box_city_game.h"

PROFILE_DEFINE_MARKER(g_profile_marker_UpdatePosition, "Main", 0xFFFFAAAA, "BoxUpdate");
PROFILE_DEFINE_MARKER(g_profile_marker_Culling, "Main", 0xFFFFAAAA, "BoxCulling");

namespace
{
	struct ViewConstantBuffer
	{
		glm::mat4x4 projection_view_matrix;
		glm::vec4 time;
		glm::vec4 sun_direction;
	};
}

void BoxCityGame::OnInit()
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

	SetRenderSystem(m_render_system);

	//Register gpu memory render module
	render::GPUMemoryRenderModule::GPUMemoryDesc gpu_memory_desc;
	gpu_memory_desc.static_gpu_memory_size = 20 * 1024 * 1024;
	gpu_memory_desc.dynamic_gpu_memory_size = 10 * 1024 * 1024;
	gpu_memory_desc.dynamic_gpu_memory_segment_size = 256 * 1024;

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
	database_desc.num_max_entities_zone = 1024 * 1024;
	database_desc.num_zones = m_tile_manager.GetNumTiles();
	ecs::CreateDatabase<GameDatabase>(database_desc);

	m_tile_manager.Build(m_device, m_render_system, m_GPU_memory_render_module);
}
	
void BoxCityGame::OnPrepareDestroy()
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

void BoxCityGame::OnDestroy()
{
	//Destroy constant buffer
	display::DestroyHandle(m_device, m_view_constant_buffer);

	//Destroy handles
	m_display_resources.Unload(m_device);

	//Destroy device
	display::DestroyDevice(m_device);
}

void BoxCityGame::OnTick(double total_time, float elapsed_time)
{
	//Reset job allocators
	m_update_job_allocator->Clear();

	//UPDATE GAME

	//Update camera
	m_camera.UpdateAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
	m_camera.Update(this, elapsed_time);

	job::Fence update_fence;

	//Update all positions for testing the static gpu memory
	ecs::AddJobs<GameDatabase, OBBBox, AABBBox, FlagBox, AnimationBox>(m_job_system, update_fence, m_update_job_allocator, 256,
		[total_time](const auto& instance_iterator, OBBBox& obb_box, AABBBox& aabb_box, FlagBox& flags, AnimationBox& animation_box)
		{
			//Update position
			obb_box.position = animation_box.original_position + glm::row(obb_box.rotation, 2) * animation_box.range * static_cast<float> (cos(total_time * animation_box.frecuency + animation_box.offset));

			//Update AABB
			helpers::CalculateAABBFromOBB(aabb_box, obb_box);

			//Mark flags to indicate that the GPU needs to update
			flags.gpu_updated = false;
		}, m_tile_manager.GetCameraBitSet(m_camera), &g_profile_marker_UpdatePosition);

	job::Wait(m_job_system, update_fence);

	render::BeginPrepareRender(m_render_system);

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
	view_constant_buffer.sun_direction = glm::rotate(glm::radians(m_sun_direction_angles.y), glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(glm::radians(m_sun_direction_angles.x), glm::vec3(0.f, 0.f, 1.f)) * glm::vec4(1.f, 0.f, 0.f, 0.f);
	render_frame.GetBeginFrameCommandBuffer().UploadResourceBuffer(m_view_constant_buffer, &view_constant_buffer, sizeof(view_constant_buffer));
	render_frame.GetBeginFrameCommandBuffer().Close();

	//Add render passes
	render_frame.AddRenderPass("Main"_sh32, 0, pass_info, "Main"_sh32, 0);
	render_frame.AddRenderPass("SyncStaticGPUMemory"_sh32, 0, pass_info);

	//Add point of view
	auto& point_of_view = render_frame.AllocPointOfView("Main"_sh32, 0);

	job::Fence culling_fence;

	//Add task
	//Cull box city
	ecs::AddJobs<GameDatabase, const OBBBox, const AABBBox, FlagBox, const BoxGPUHandle>(m_job_system, culling_fence, m_update_job_allocator, 256,
		[camera = &m_camera, point_of_view = &point_of_view, box_priority = m_box_render_priority, render_system = m_render_system, device = m_device, render_gpu_memory_module = m_GPU_memory_render_module]
	(const auto& instance_iterator, const OBBBox& obb_box, const AABBBox& aabb_box, FlagBox& flags, const BoxGPUHandle& box_gpu_handle)
		{
			//Calculate if it is in the camera
			if (helpers::CollisionFrustumVsAABB(*camera, aabb_box))
			{
				//Update GPU if needed
				if (!flags.gpu_updated)
				{
					GPUBoxInstance gpu_box_instance;
					gpu_box_instance.Fill(obb_box);

					render_gpu_memory_module->UpdateStaticGPUMemory(device, box_gpu_handle.gpu_memory, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(render_system));
					flags.gpu_updated = true;
				}

				//Calculate sort key, sort key is 24 bits
				float camera_distance = glm::length(obb_box.position - camera->GetPosition());
				float camera_distance_01 = glm::clamp(camera_distance, 0.f, camera->GetFarPlane()) / camera->GetFarPlane();
				uint32_t sort_key = static_cast<uint32_t>(camera_distance_01 * (1 << 24));

				//Add this point of view
				point_of_view->PushRenderItem(box_priority, static_cast<render::SortKey>(sort_key), static_cast<uint32_t>(render_gpu_memory_module->GetStaticGPUMemoryOffset(box_gpu_handle.gpu_memory)));
			}

		}, m_tile_manager.GetCameraBitSet(m_camera), &g_profile_marker_Culling);

	job::Wait(m_job_system, culling_fence);

	//Render
	render::EndPrepareRenderAndSubmit(m_render_system);

	{
		PROFILE_SCOPE("ECSTest", 0xFFFF77FF, "DatabaseTick");
		//Tick database
		ecs::Tick<GameDatabase>();
	}
}

void BoxCityGame::OnSizeChange(uint32_t width, uint32_t height, bool minimized)
{
	m_width = width;
	m_height = height;

	if (m_render_system)
	{
		render::GetResource<render::RenderTargetResource>(m_render_system, "BackBuffer"_sh32)->UpdateInfo(width, height);
	}
}

void BoxCityGame::OnAddImguiMenu()
{
	//Add menu for modifying the render system descriptor file
	if (ImGui::BeginMenu("BoxCity"))
	{
		m_render_passes_loader.GetShowEditDescriptorFile() = ImGui::MenuItem("Edit descriptor file");
		bool single_frame_mode = job::GetSingleThreadMode(m_job_system);
		if (ImGui::Checkbox("Single thread mode", &single_frame_mode))
		{
			job::SetSingleThreadMode(m_job_system, single_frame_mode);
		}
		ImGui::SliderFloat2("Sun Direction", reinterpret_cast<float*>(&m_sun_direction_angles), 0.f, 360.f);
		ImGui::EndMenu();
	}
}

void BoxCityGame::OnImguiRender()
{
	m_render_passes_loader.RenderImgui();
}
