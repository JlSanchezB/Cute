#include "box_city_game.h"
#include <core/counters.h>

PROFILE_DEFINE_MARKER(g_profile_marker_UpdatePosition, "Main", 0xFFFFAAAA, "BoxUpdate");
PROFILE_DEFINE_MARKER(g_profile_marker_UpdateAttachments, "Main", 0xFFFFAAAA, "BoxAttachment");
PROFILE_DEFINE_MARKER(g_profile_marker_Culling, "Main", 0xFFFFAAAA, "BoxCulling");
PROFILE_DEFINE_MARKER(g_profile_marker_Car_Culling, "Main", 0xFFFFAAAA, "CarCulling");

COUNTER(c_Culled_Boxes, "Box City", "Render Boxes", true);
COUNTER(c_Culled_Cars, "Box City", "Render Cars", true);

void BoxCityGame::OnInit()
{
	//Setup the tick to 60fps logic tick and render
	SetUpdateType(platform::UpdateType::LogicRender, 60.f);

	display::DeviceInitParams device_init_params;

#ifdef _DEBUG
	device_init_params.debug = true;
#else
	device_init_params.debug = true;
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

	//Create job system
	job::SystemDesc job_system_desc;
	m_job_system = job::CreateSystem(job_system_desc);

	//Create Job allocator now that the job system is enabled
	m_update_job_allocator = std::make_unique<job::JobAllocator<1024 * 1024>>();
	m_render_job_allocator = std::make_unique<job::JobAllocator<1024 * 1024>>();

	//Create render pass system
	render::SystemDesc render_system_desc;
	m_render_system = render::CreateRenderSystem(m_device, m_job_system, this, render_system_desc);

	SetRenderSystem(m_render_system);

	//Register gpu memory render module
	render::GPUMemoryRenderModule::GPUMemoryDesc gpu_memory_desc;
	gpu_memory_desc.static_gpu_memory_size = 50 * 1024 * 1024;
	gpu_memory_desc.dynamic_gpu_memory_size = 200 * 1024 * 1024;
	gpu_memory_desc.dynamic_gpu_memory_segment_size = 5 * 1024 * 1024;

	m_GPU_memory_render_module = render::RegisterModule<render::GPUMemoryRenderModule>(m_render_system, "GPUMemory"_sh32, gpu_memory_desc);

	m_display_resources.Load(m_device, m_render_system);
	DrawCityBoxesPass::m_display_resources = &m_display_resources;
	CullCityBoxesPass::m_display_resources = &m_display_resources;

	//Register custom passes for box city renderer
	render::RegisterPassFactory<DrawCityBoxesPass>(m_render_system);
	render::RegisterPassFactory<CullCityBoxesPass>(m_render_system);

	//Register the ViewConstantBuffer for Main pass, ID 0
	render::AddGameResource(m_render_system, "ViewConstantBuffer"_sh32, "Main"_sh32, 0, CreateResourceFromHandle<render::ConstantBufferResource>(display::WeakConstantBufferHandle(m_display_resources.m_view_constant_buffer)));
	render::AddGameResource(m_render_system, "IndirectBoxBuffer"_sh32, CreateResourceFromHandle<render::UnorderedAccessBufferResource>(display::WeakUnorderedAccessBufferHandle(m_display_resources.m_indirect_box_buffer)));
	render::AddGameResource(m_render_system, "IndirectParametersBuffer"_sh32, CreateResourceFromHandle<render::UnorderedAccessBufferResource>(display::WeakUnorderedAccessBufferHandle(m_display_resources.m_indirect_parameters_buffer)));

	m_render_passes_loader.Load("box_city_render_passes.xml", m_render_system, m_device);

	//Get render priorities
	m_box_render_priority = render::GetRenderItemPriority(m_render_system, "Box"_sh32);

	//Create ecs database
	ecs::DatabaseDesc database_desc;
	database_desc.num_max_entities_zone = 1024 * 1024;
	database_desc.num_zones = m_tile_manager.GetNumTiles();
	ecs::CreateDatabase<GameDatabase>(database_desc);

	m_fly_camera.SetNearFar(0.5f, 8000.f);
	m_car_camera.SetNearFar(0.5f, 8000.f);

	m_tile_manager.Init(m_device, m_render_system, m_GPU_memory_render_module);
	m_traffic_system.Init(m_device, m_render_system, m_GPU_memory_render_module);

	//Register the callback transaction function in the ecs
	ecs::RegisterCallbackTransaction<GameDatabase>([manager = &m_traffic_system](const ecs::DababaseTransaction transaction, const ecs::ZoneType zone, const ecs::EntityTypeType entity_type, const ecs::InstanceIndexType instance_index, const ecs::ZoneType zone_ext, const ecs::EntityTypeType entity_type_ext, const ecs::InstanceIndexType instance_index_ext)
		{
			if (GameDatabase::EntityTypeIndex<CarType>() == entity_type)
			{
				manager->RegisterECSChange(static_cast<uint32_t>(zone), static_cast<uint32_t>(instance_index));
				if (transaction == ecs::DababaseTransaction::Move)
				{
					manager->RegisterECSChange(static_cast<uint32_t>(zone_ext), static_cast<uint32_t>(instance_index_ext));
				}
			}
		});
}
	
void BoxCityGame::OnPrepareDestroy()
{
	//Destroy tile manager
	m_tile_manager.Shutdown();

	//Destroy traffic manager
	m_traffic_system.Shutdown();

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
	//Destroy handles
	m_display_resources.Unload(m_device);

	//Destroy device
	display::DestroyDevice(m_device);
}

void BoxCityGame::OnLogic(double total_time, float elapsed_time)
{
	if (!IsWindowFocus() && m_camera_mode == CameraMode::Car && m_traffic_system.GetPlayerControlEnable())
	{
		//Skip update
		return;
	}

	//Reset job allocators
	m_update_job_allocator->Clear();

	//UPDATE GAME

	//Check the camera mode
	for (auto& input_event : GetInputEvents())
	{
		if (input_event.type == platform::EventType::KeyDown && input_event.slot == platform::InputSlotState::Key_1)
		{
			m_camera_mode = CameraMode::Fly;
			m_traffic_system.SetPlayerControlEnable(false);
			ReleaseMouse();
			ShowCursor(true);
		}
		if (input_event.type == platform::EventType::KeyDown && input_event.slot == platform::InputSlotState::Key_2)
		{
			m_camera_mode = CameraMode::Car;
			m_traffic_system.SetPlayerControlEnable(true);
			CaptureMouse();
			ShowCursor(false);
		}
		if (input_event.type == platform::EventType::KeyDown && input_event.slot == platform::InputSlotState::Key_3)
		{
			m_camera_mode = CameraMode::Car;
			m_traffic_system.SetPlayerControlEnable(false);
			ReleaseMouse();
			ShowCursor(true);
		}
	}

	//Update camera, for logic update
	helpers::Camera* camera = dynamic_cast<helpers::Camera*>(&m_fly_camera);
	
	switch (m_camera_mode)
	{
	case CameraMode::Fly:
		camera = dynamic_cast<helpers::Camera*>(&m_fly_camera);
		break;
	case CameraMode::Car:
		camera = dynamic_cast<helpers::Camera*>(&m_car_camera);
		break;
	}
	
	//Update tile manager
	m_tile_manager.Update(camera->GetPosition());

	//Update traffic manager
	m_traffic_system.Update(&m_tile_manager, camera->GetPosition());

	job::Fence update_fence;

	//Update all positions for testing the static gpu memory
	ecs::AddJobs<GameDatabase, OBBBox, FlagBox, AnimationBox, InterpolatedPosition>(m_job_system, update_fence, m_update_job_allocator, 256,
		[total_time](const auto& instance_iterator, OBBBox& obb_box, FlagBox& flags, AnimationBox& animation_box, InterpolatedPosition& interpolated_position)
		{
			//Update position in the OBB, AABB represent the extended version, so it doesn't need to be updated
			*interpolated_position.position = animation_box.original_position + glm::row(obb_box.rotation, 2) * animation_box.range * static_cast<float> (cos(total_time * animation_box.frecuency + animation_box.offset));
			obb_box.position = *interpolated_position.position;

			//Mark flags to indicate that the GPU needs to update
			flags.gpu_updated = false;
		}, m_tile_manager.GetCameraBitSet(*camera), &g_profile_marker_UpdatePosition);

	job::Wait(m_job_system, update_fence);

	job::Fence update_attachments_fence;
	//Update attachments
	ecs::AddJobs<GameDatabase, OBBBox, AABBBox, FlagBox, Attachment, InterpolatedPosition>(m_job_system, update_attachments_fence, m_update_job_allocator, 256,
		[total_time](const auto& instance_iterator, OBBBox& obb_box, AABBBox& aabb_box, FlagBox& flags, Attachment& attachment, InterpolatedPosition& interpolated_position)
		{
			//Get parent matrix
			OBBBox& parent_obb = attachment.parent.Get<GameDatabase>().Get<OBBBox>();

			glm::mat4x4 box_to_world(parent_obb.rotation);
			box_to_world[3] = glm::vec4(parent_obb.position, 1.f);

			glm::mat4x4 panel_matrix = box_to_world * attachment.parent_to_child;

			//The position is the only one to interpolate
			*interpolated_position.position = panel_matrix[3];

			obb_box.position = panel_matrix[3];
			obb_box.rotation = glm::mat3x3(panel_matrix);
			
			//Update AABB
			helpers::CalculateAABBFromOBB(aabb_box, obb_box);

			//Mark flags to indicate that the GPU needs to update
			flags.gpu_updated = false;
		}, m_tile_manager.GetCameraBitSet(*camera), &g_profile_marker_UpdateAttachments);

	//Update cars
	job::Fence update_cars_fence;
	m_traffic_system.UpdateCars(this, m_job_system, m_update_job_allocator.get(), *camera, update_cars_fence, &m_tile_manager, m_frame_index, elapsed_time);

	job::Wait(m_job_system, update_attachments_fence);
	job::Wait(m_job_system, update_cars_fence);

	//Update camera for the render frame or next logic update
	switch (m_camera_mode)
	{
	case CameraMode::Fly:
		m_fly_camera.Update(this, elapsed_time);
		break;
	case CameraMode::Car:
		if (m_traffic_system.GetPlayerCar().IsValid())
		{
			m_car_camera.Update(this, m_traffic_system.GetPlayerCar().Get<GameDatabase>().Get<Car>(), elapsed_time);
		}
		break;
	}

	{
		PROFILE_SCOPE("BoxCity", 0xFFFF77FF, "DatabaseTick");
		//Tick database
		ecs::Tick<GameDatabase>();
	}

	m_frame_index++;
}

void BoxCityGame::OnRender(double total_time, float elapsed_time)
{
	{
		PROFILE_SCOPE("BoxCity", 0xFFFF77FF, "UpdateTrafficInstancesLists");
		//Process all the moves for all the updates and update the instance lists correctly (acumulated all the frames before)
		m_traffic_system.ProcessCarMoves();
	}

	//Reset job allocators
	m_render_job_allocator->Clear();

	render::BeginPrepareRender(m_render_system);

	//Update camera, for rendering just call update render that will interpolate the positions
	helpers::Camera* camera = dynamic_cast<helpers::Camera*>(&m_fly_camera);
	switch (m_camera_mode)
	{
	case CameraMode::Fly:
		camera = dynamic_cast<helpers::Camera*>(&m_fly_camera);
		m_fly_camera.UpdateAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
		m_fly_camera.UpdateRender();
		break;
	case CameraMode::Car:
		camera = dynamic_cast<helpers::Camera*>(&m_car_camera);
		m_car_camera.UpdateAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
		m_car_camera.UpdateRender();
		break;
	}

	//Check if the render passes loader needs to load
	m_render_passes_loader.Update();

	render::PassInfo pass_info;
	pass_info.Init(m_width, m_height);

	render::Frame& render_frame = render::GetGameRenderFrame(m_render_system);

	//Update view constant buffer with the camera and the time
	auto command_offset = render_frame.GetBeginFrameCommandBuffer().Open();
	ViewConstantBuffer view_constant_buffer;
	view_constant_buffer.projection_view_matrix = camera->GetViewProjectionMatrix();
	view_constant_buffer.time = glm::vec4(static_cast<float>(total_time), 0.f, 0.f, 0.f);
	view_constant_buffer.sun_direction = glm::rotate(glm::radians(m_sun_direction_angles.y), glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(glm::radians(m_sun_direction_angles.x), glm::vec3(0.f, 0.f, 1.f)) * glm::vec4(1.f, 0.f, 0.f, 0.f);
	for (size_t i = 0; i < helpers::Frustum::Count; ++i) view_constant_buffer.frustum_planes[i] = camera->planes[i];
	for (size_t i = 0; i < 8; ++i) view_constant_buffer.frustum_points[i] = glm::vec4(camera->points[i], 1.f);
	
	render_frame.GetBeginFrameCommandBuffer().UploadResourceBuffer(m_display_resources.m_view_constant_buffer, &view_constant_buffer, sizeof(view_constant_buffer));
	render_frame.GetBeginFrameCommandBuffer().Close();

	//Add render passes
	render_frame.AddRenderPass("Main_Render"_sh32, 0, pass_info, "Main_Render"_sh32, 0);
	render_frame.AddRenderPass("Main_Culling"_sh32, 0, pass_info, "Main_Render"_sh32, 0);
	render_frame.AddRenderPass("SyncStaticGPUMemory"_sh32, 0, pass_info);

	//Fill the custom data for the point of view
	BoxCityCustomPointOfViewData point_of_view_data;

	{
		//Collect all the active tiles and build the list of instance_lists that needs to be processed by the GPU
		std::vector<uint32_t> instance_list_offsets_array;
		m_tile_manager.AppendVisibleInstanceLists(*camera, instance_list_offsets_array);
		m_traffic_system.AppendVisibleInstanceLists(*camera, instance_list_offsets_array);

		//Allocate dynamic gpu memory to upload the instance_list_offsets_array
		size_t buffer_size = render::RoundSizeUp16Bytes((instance_list_offsets_array.size() + 1) * sizeof(uint32_t));

		uint32_t* gpu_instance_list_offset_array = reinterpret_cast<uint32_t*>(m_GPU_memory_render_module->AllocDynamicGPUMemory(m_device, buffer_size, render::GetGameFrameIndex(m_render_system)));

		//First the count
		gpu_instance_list_offset_array[0] = static_cast<uint32_t>(instance_list_offsets_array.size());
		//Then the list of offsets
		memcpy(&gpu_instance_list_offset_array[1], instance_list_offsets_array.data(), instance_list_offsets_array.size() * sizeof(uint32_t));

		//Update the point of view data
		point_of_view_data.instance_lists_offset = static_cast<uint32_t>(m_GPU_memory_render_module->GetDynamicGPUMemoryOffset(m_device, gpu_instance_list_offset_array));
		point_of_view_data.num_instance_lists = static_cast<uint32_t>(instance_list_offsets_array.size());
	}

	//Add point of view
	auto& point_of_view = render_frame.AllocPointOfView<BoxCityCustomPointOfViewData>("Main_Render"_sh32, 0, point_of_view_data);

	job::Fence culling_fence;
	uint64_t render_frame_index = render::GetGameFrameIndex(m_render_system);

	//Add task
	//Interpolate animation
	ecs::AddJobs<GameDatabase, const OBBBox, const InterpolatedPosition, const BoxGPUHandle>(m_job_system, culling_fence, m_render_job_allocator, 256,
		[camera = camera, render_system = m_render_system, device = m_device, render_gpu_memory_module = m_GPU_memory_render_module, tile_manager = &m_tile_manager, render_frame_index]
	(const auto& instance_iterator, const OBBBox obb_box,const InterpolatedPosition interpolated_position, const BoxGPUHandle& box_gpu_handle)
		{
			//Calculate if it is in the camera and has still a gpu memory handle
			if (box_gpu_handle.IsValid())
			{
				//Get GPU alloc handle
				render::AllocHandle& gpu_handle = tile_manager->GetGPUHandle(instance_iterator.m_zone_index, box_gpu_handle.lod_group);

				//We need to interpolate the position of the oobb for rendering
				OBBBox render_obb = obb_box;
				render_obb.position = instance_iterator.Get<InterpolatedPosition>().position.GetInterpolated();

				GPUBoxInstance gpu_box_instance;
				gpu_box_instance.Fill(render_obb);

				//Only the first 3 float4
				render_gpu_memory_module->UpdateStaticGPUMemory(device, gpu_handle, &gpu_box_instance, 3 * sizeof(glm::vec4), render_frame_index, box_gpu_handle.offset_gpu_allocator * sizeof(GPUBoxInstance));
			}
		}, m_tile_manager.GetCameraBitSet(*camera), &g_profile_marker_Culling);

	//Interpolate cars
	ecs::AddJobs<GameDatabase, const OBBBox, const AABBBox, const CarGPUIndex, const Car>(m_job_system, culling_fence, m_render_job_allocator, 256,
		[camera = camera, render_system = m_render_system, device = m_device, render_gpu_memory_module = m_GPU_memory_render_module, traffic_manager = &m_traffic_system, render_frame_index]
	(const auto& instance_iterator, const OBBBox& obb_box, const AABBBox& aabb_box, const CarGPUIndex& car_gpu_index, const Car& car)
		{
			if (car_gpu_index.IsValid())
			{
				//Calculate the interpolated OBB
				OBBBox render_box = obb_box;
				render_box.position = car.position.GetInterpolated();
				render_box.rotation = glm::toMat3(car.rotation.GetInterpolated());

				//Update GPU
				GPUBoxInstance gpu_box_instance;
				gpu_box_instance.Fill(render_box);

				//Only the first 3 float4
				render_gpu_memory_module->UpdateStaticGPUMemory(device, traffic_manager->GetGPUHandle(), &gpu_box_instance, 3 * sizeof(glm::vec4), render_frame_index, car_gpu_index.gpu_slot * sizeof(GPUBoxInstance));
			}
		}, m_traffic_system.GetCameraBitSet(*camera), & g_profile_marker_Car_Culling);

	job::Wait(m_job_system, culling_fence);

	//Render
	render::EndPrepareRenderAndSubmit(m_render_system);
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
		m_show_ecs_stats = ImGui::MenuItem("Show ECS stats");
		ImGui::EndMenu();
	}
}

void BoxCityGame::OnImguiRender()
{
	m_render_passes_loader.RenderImgui();

	if (m_show_ecs_stats)
	{
		if (!ImGui::Begin("Show ECS stats", &m_show_ecs_stats))
		{
			ImGui::End();
			return;
		}
		ImGui::Text("Num city boxes (%zu)", ecs::GetNumInstances<GameDatabase, BoxType>());
		ImGui::Text("Num animated city boxes (%zu)", ecs::GetNumInstances<GameDatabase, AnimatedBoxType>());
		ImGui::Text("Num city panels (%zu)", ecs::GetNumInstances<GameDatabase, PanelType>());
		ImGui::Text("Num attached city panels (%zu)", ecs::GetNumInstances<GameDatabase, AttachedPanelType>());
		ImGui::Text("Num cars (%zu)", ecs::GetNumInstances<GameDatabase, CarType>());

		ecs::DatabaseStats database_stats;
		ecs::GetDatabaseStats<GameDatabase>(database_stats);

		ImGui::Separator();
		ImGui::Text("Num deferred deletions (%zu)", database_stats.num_deferred_deletions);
		ImGui::Text("Num deferred moves (%zu)", database_stats.num_deferred_moves);

		ImGui::End();
	}
}
