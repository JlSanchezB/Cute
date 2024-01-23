#include "box_city_traffic_manager.h"
#include <core/profile.h>
#include "box_city_tile_manager.h"
#include <render/render.h>
#include <ecs/entity_component_system.h>
#include <ecs/zone_bitmask_helper.h>
#include <core/profile.h>
#include <job/job.h>
#include <job/job_helper.h>
#include <ecs/entity_component_job_helper.h>
#include <core/control_variables.h>
#include <core/counters.h>
#include "box_city_car_control.h"

PROFILE_DEFINE_MARKER(g_profile_marker_Car_Update, "Main", 0xFFFFAAAA, "CarUpdate");
CONTROL_VARIABLE_BOOL(c_traffic_full_instance_list_upload, false, "TrafficSystem", "Upload all invalidated instance_list");
COUNTER(c_Car_Summitted, "Box City", "Car summitted to the GPU", true);

namespace BoxCityTrafficSystem
{
	thread_local std::mt19937 random_thread_local(std::random_device{}());

	void Manager::Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module)
	{
		m_device = device;
		m_render_system = render_system;
		m_GPU_memory_render_module = GPU_memory_render_module;

		m_camera_tile_position.i = std::numeric_limits<int32_t>::max();
		m_camera_tile_position.j = std::numeric_limits<int32_t>::max();

		//Generate zone descriptors
		GenerateZoneDescriptors();

		//Init tiles
		for (uint32_t i = 0; i < kLocalTileCount * kLocalTileCount; ++i)
		{
			m_tiles[i].m_zone_index = i;
		}

		//Allocate GPU memory, num tiles * max num cars
		m_gpu_memory = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, kNumCars * kLocalTileCount * kLocalTileCount * sizeof(GPUBoxInstance), nullptr, render::GetGameFrameIndex(m_render_system));

		std::vector<uint8_t> car_buffer;
		auto fill_box = [&](uint32_t index, const glm::vec3& position, const glm::vec3& extent, const glm::vec3& colour, const bool emissive)
		{
			GPUBox* gpu_box = reinterpret_cast<GPUBox*>(&car_buffer[16 + index * sizeof(GPUBox)]);
			gpu_box->Fill(position, extent, colour, (emissive) ? GPUBox::kFlags_Emissive : 0);
		};

		float emissive_factor = 3.f;

		//Create the car box list, car 0
		{
			const uint32_t num_boxes = 10;
			car_buffer.resize(16 + num_boxes * sizeof(GPUBox));
			uint32_t* buffer_size = reinterpret_cast<uint32_t*>(car_buffer.data());
			buffer_size[0] = num_boxes;
			buffer_size[1] = 0;
			buffer_size[2] = 0;
			buffer_size[3] = 0;

			fill_box(0, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.95f, 0.95f, 0.3f), glm::vec3(1.f, 1.f, 1.f), false);

			//Top
			fill_box(1, glm::vec3(0.f, -0.1f, 0.5f), glm::vec3(0.8f, 0.45f, 0.2f), glm::vec3(0.3f, 1.0f, 1.f), false);

			//Bottom engines
			fill_box(2, glm::vec3(0.7f, 0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f) , glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(3, glm::vec3(0.7f, -0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(4, glm::vec3(-0.7f, 0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(5, glm::vec3(-0.7f, -0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);

			//Front lights
			fill_box(6, glm::vec3(0.6f, 0.975f, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(8.f, 8.f, 2.f) * emissive_factor, true);
			fill_box(7, glm::vec3(-0.6f, 0.975f, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(8.f, 8.f, 2.f) * emissive_factor, true);

			//Rear lights
			fill_box(8, glm::vec3(0.6f, -0.975f, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(5.f, 0.f, 0.f) * emissive_factor, true);
			fill_box(9, glm::vec3(-0.6f, -0.975f, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(5.f, 0.f, 0.f) * emissive_factor, true);

			//Allocate GPU car box list
			m_gpu_car_box_list[0] = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, car_buffer.size(), car_buffer.data(), render::GetGameFrameIndex(m_render_system));
		}
		//Create the car box list, car 1
		{
			const uint32_t num_boxes = 10;
			const float length_factor = 2.f;
			car_buffer.resize(16 + num_boxes * sizeof(GPUBox));
			uint32_t* buffer_size = reinterpret_cast<uint32_t*>(car_buffer.data());
			buffer_size[0] = num_boxes;
			buffer_size[1] = 0;
			buffer_size[2] = 0;
			buffer_size[3] = 0;

			fill_box(0, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.95f, 0.95f * length_factor, 0.3f), glm::vec3(1.f, 1.f, 1.f), false);

			//Top
			fill_box(1, glm::vec3(0.f, 0.0f, 0.55f), glm::vec3(0.95f, 0.95f * length_factor, 0.3f), glm::vec3(0.3f, 1.0f, 1.f), false);

			//Bottom engines
			fill_box(2, glm::vec3(0.7f, 0.7f * length_factor, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(3, glm::vec3(0.7f, -0.7f * length_factor, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(4, glm::vec3(-0.7f, 0.7f * length_factor, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(5, glm::vec3(-0.7f, -0.7f * length_factor, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);

			//Front lights
			fill_box(6, glm::vec3(0.6f, 0.975f * length_factor, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(8.f, 8.f, 2.f) * emissive_factor, true);
			fill_box(7, glm::vec3(-0.6f, 0.975f * length_factor, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(8.f, 8.f, 2.f) * emissive_factor, true);

			//Rear lights
			fill_box(8, glm::vec3(0.6f, -0.975f * length_factor, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(5.f, 0.f, 0.f) * emissive_factor, true);
			fill_box(9, glm::vec3(-0.6f, -0.975f * length_factor, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(5.f, 0.f, 0.f) * emissive_factor, true);

			//Allocate GPU car box list
			m_gpu_car_box_list[1] = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, car_buffer.size(), car_buffer.data(), render::GetGameFrameIndex(m_render_system));
		}
		//Create the car box list, car 2
		{
			const uint32_t num_boxes = 10;
			car_buffer.resize(16 + num_boxes * sizeof(GPUBox));
			uint32_t* buffer_size = reinterpret_cast<uint32_t*>(car_buffer.data());
			buffer_size[0] = num_boxes;
			buffer_size[1] = 0;
			buffer_size[2] = 0;
			buffer_size[3] = 0;

			fill_box(0, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.75f, 0.95f, 0.3f), glm::vec3(1.f, 0.3f, 0.3f), false);

			//Top
			fill_box(1, glm::vec3(0.f, -0.1f, 0.5f), glm::vec3(0.5f, 0.45f, 0.2f), glm::vec3(0.3f, 1.0f, 1.f), false);

			//Bottom engines
			fill_box(2, glm::vec3(0.7f, 0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(3, glm::vec3(0.7f, -0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(4, glm::vec3(-0.7f, 0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(5, glm::vec3(-0.7f, -0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);

			//Front lights
			fill_box(6, glm::vec3(0.6f, 0.975f, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(8.f, 8.f, 2.f) * emissive_factor, true);
			fill_box(7, glm::vec3(-0.6f, 0.975f, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(8.f, 8.f, 2.f) * emissive_factor, true);

			//Rear lights
			fill_box(8, glm::vec3(0.6f, -0.975f, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(5.f, 0.f, 0.f) * emissive_factor, true);
			fill_box(9, glm::vec3(-0.6f, -0.975f, 0.f), glm::vec3(0.2f, 0.025f, 0.2f), glm::vec3(5.f, 0.f, 0.f) * emissive_factor, true);

			//Allocate GPU car box list
			m_gpu_car_box_list[2] = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, car_buffer.size(), car_buffer.data(), render::GetGameFrameIndex(m_render_system));
		}
		//Create the car box list, car 3
		{
			const uint32_t num_boxes = 8;
			car_buffer.resize(16 + num_boxes * sizeof(GPUBox));
			uint32_t* buffer_size = reinterpret_cast<uint32_t*>(car_buffer.data());
			buffer_size[0] = num_boxes;
			buffer_size[1] = 0;
			buffer_size[2] = 0;
			buffer_size[3] = 0;

			fill_box(0, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.95f, 0.95f, 0.3f), glm::vec3(0.2f, 0.2f, 1.f), false);

			//Top
			fill_box(1, glm::vec3(0.f, -0.1f, 0.5f), glm::vec3(0.8f, 0.45f, 0.2f), glm::vec3(0.3f, 1.0f, 1.f), false);

			//Bottom engines
			fill_box(2, glm::vec3(0.7f, 0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(3, glm::vec3(0.7f, -0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(4, glm::vec3(-0.7f, 0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(5, glm::vec3(-0.7f, -0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);

			//Front lights
			fill_box(6, glm::vec3(0.0f, 0.975f, 0.f), glm::vec3(0.7f, 0.025f, 0.2f), glm::vec3(8.f, 8.f, 2.f) * emissive_factor, true);

			//Rear lights
			fill_box(7, glm::vec3(0.0f, -0.975f, 0.f), glm::vec3(0.7f, 0.025f, 0.2f), glm::vec3(5.f, 0.f, 0.f) * emissive_factor, true);
			

			//Allocate GPU car box list
			m_gpu_car_box_list[3] = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, car_buffer.size(), car_buffer.data(), render::GetGameFrameIndex(m_render_system));
		}
		//Create the car box list, car 4
		{
			const uint32_t num_boxes = 8;
			car_buffer.resize(16 + num_boxes * sizeof(GPUBox));
			uint32_t* buffer_size = reinterpret_cast<uint32_t*>(car_buffer.data());
			buffer_size[0] = num_boxes;
			buffer_size[1] = 0;
			buffer_size[2] = 0;
			buffer_size[3] = 0;

			fill_box(0, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.65f, 0.65f, 0.3f), glm::vec3(1.0f, 1.0f, 0.2f), false);

			//Top
			fill_box(1, glm::vec3(0.f, -0.1f, 0.5f), glm::vec3(0.5f, 0.45f, 0.2f), glm::vec3(0.3f, 1.0f, 1.f), false);

			//Bottom engines
			fill_box(2, glm::vec3(0.7f, 0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(3, glm::vec3(0.7f, -0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(4, glm::vec3(-0.7f, 0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);
			fill_box(5, glm::vec3(-0.7f, -0.7f, -0.35f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(3.f, 1.f, 1.f) * emissive_factor, true);

			//Front lights
			fill_box(6, glm::vec3(0.0f, 0.975f, 0.f), glm::vec3(0.5f, 0.025f, 0.2f), glm::vec3(8.f, 8.f, 2.f) * emissive_factor, true);

			//Rear lights
			fill_box(7, glm::vec3(0.0f, -0.975f, 0.f), glm::vec3(0.5f, 0.025f, 0.2f), glm::vec3(5.f, 0.f, 0.f) * emissive_factor, true);


			//Allocate GPU car box list
			m_gpu_car_box_list[4] = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, car_buffer.size(), car_buffer.data(), render::GetGameFrameIndex(m_render_system));
		}
		

	}

	void Manager::Shutdown()
	{
		//Deallocate the tile GPU memory
		for (auto& tile : m_tiles)
		{
			if (tile.m_instances_list_handle.IsValid())
			{
				//Deallocate GPU memory
				m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, tile.m_instances_list_handle, render::GetGameFrameIndex(m_render_system));
			}
		}

		//Deallocate GPU memory
		m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, m_gpu_memory, render::GetGameFrameIndex(m_render_system));
		m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, m_gpu_car_box_list[0], render::GetGameFrameIndex(m_render_system));
		m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, m_gpu_car_box_list[1], render::GetGameFrameIndex(m_render_system));
		m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, m_gpu_car_box_list[2], render::GetGameFrameIndex(m_render_system));
		m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, m_gpu_car_box_list[3], render::GetGameFrameIndex(m_render_system));
		m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, m_gpu_car_box_list[4], render::GetGameFrameIndex(m_render_system));
	}

	void Manager::SetupCar(Tile& tile, std::mt19937& random, float begin_tile_x, float begin_tile_y,
		std::uniform_real_distribution<float>& position_range, std::uniform_real_distribution<float>& position_range_z, std::uniform_real_distribution<float>& size_range,
		Car& car, CarMovement& car_movement, CarSettings& car_settings, OBBBox& obb_component, CarGPUIndex& car_gpu_index, CarBoxListOffset& car_box_list_offset)
	{
		glm::vec3 position(begin_tile_x + position_range(random), begin_tile_y + position_range(random), position_range_z(random));
		
		//Car type
		uint32_t car_type = (random() % 5);
		
		auto car_dimensions = std::array{	glm::vec3(1.0f, 1.f, 0.7f),
											glm::vec3(1.0f, 3.f, 0.9f),
											glm::vec3(1.0f, 1.0f, 0.6f), 
											glm::vec3(1.0f, 1.1f, 0.7f),
											glm::vec3(1.1f, 1.f, 0.7f) };

		float size = size_range(random);
		
		car.position.Reset(position);
		car.rotation.Reset(glm::quat(glm::vec3(0.f, 0.f, 0.f)));
		car_movement.lineal_velocity = glm::vec3(0.f, 0.f, 0.f);
		car_movement.rotation_velocity = glm::vec3(0.f, 0.f, 0.f);

		car_settings.size = size;
		glm::vec3 car_size = car_dimensions[car_type] * size;
		float mass = (car_size.x * car_size.y * car_size.z);
		car_settings.inv_mass = 1.f / mass;
		car_settings.inv_mass_inertia = glm::vec3(
			1.f / (0.083f * mass * (car_size.z * car_size.z + car_size.y * car_size.y)),
			1.f / (0.083f * mass * (car_size.x * car_size.x + car_size.y * car_size.y)),
			1.f / (0.083f * mass * (car_size.x * car_size.x + car_size.z * car_size.z)));

		obb_component.position = position;
		obb_component.extents = car_dimensions[car_type];
		obb_component.rotation = glm::toMat3(*car.rotation);

		car_box_list_offset.car_box_list_offset = static_cast<uint32_t>(m_GPU_memory_render_module->GetStaticGPUMemoryOffset(m_gpu_car_box_list[car_type]));
		//Update GPU, all the gpu instance
		GPUBoxInstance gpu_box_instance;
		gpu_box_instance.Fill(obb_component.position, obb_component.extents, glm::toQuat(obb_component.rotation), obb_component.position, glm::toQuat(obb_component.rotation), car_box_list_offset.car_box_list_offset);

		//Update the GPU memory
		m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, m_gpu_memory, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system), car_gpu_index.gpu_slot * sizeof(GPUBoxInstance));
	}

	void Manager::Update(BoxCityTileSystem::Manager* tile_manager, const glm::vec3& camera_position)
	{
		PROFILE_SCOPE("BoxCityTrafficManager", 0xFFFF77FF, "Update");
		//Check if the camera is still in the same tile, using some fugde factor
		constexpr float fudge_factor = 0.05f;
		float min_x = (m_camera_tile_position.i - fudge_factor) * kTileSize;
		float min_y = (m_camera_tile_position.j - fudge_factor) * kTileSize;
		float max_x = (m_camera_tile_position.i + 1 + fudge_factor) * kTileSize;
		float max_y = (m_camera_tile_position.j + 1 + fudge_factor) * kTileSize;

		//If the camera has move of tile, destroy the tiles out of view and create the new ones
		bool camera_moved = (camera_position.x < min_x) || (camera_position.y < min_y) || (camera_position.x > max_x) || (camera_position.y > max_y);

		if (camera_moved)
		{
			//Update the current camera tile
			m_camera_tile_position = CalculateWorldPositionToWorldTile(camera_position);
			
			//Check if we need to create/destroy traffic
			for (auto& tile_descriptor : m_tile_descriptors)
			{
				//Calculate world tile
				WorldTilePosition world_tile{ m_camera_tile_position.i + tile_descriptor.i_offset, m_camera_tile_position.j + tile_descriptor.j_offset };

				//Calculate local tile
				LocalTilePosition local_tile = CalculateLocalTileIndex(world_tile);
				Tile& tile = GetTile(local_tile);

				//Check if the tile has the different world index
				if ((tile.m_tile_position.i != world_tile.i) || (tile.m_tile_position.j != world_tile.j))
				{
					//Tile positions
					const float begin_tile_x = world_tile.i * kTileSize;
					const float begin_tile_y = world_tile.j * kTileSize;
					
					const float kExtraTopBottomRange = 200.f;

					//Update tile
					tile.m_tile_position = world_tile;
					tile.m_bounding_box.min = glm::vec3(begin_tile_x, begin_tile_y, BoxCityTileSystem::kTileHeightBottom - kExtraTopBottomRange);
					tile.m_bounding_box.max = glm::vec3(begin_tile_x + kTileSize, begin_tile_y + kTileSize, BoxCityTileSystem::kTileHeightTop + kExtraTopBottomRange);
					
					std::mt19937 random(static_cast<uint32_t>((100000 + world_tile.i) + (100000 + world_tile.j) * kLocalTileCount));

					std::uniform_real_distribution<float> position_range(0.f, kTileSize);
					std::uniform_real_distribution<float> position_range_z(BoxCityTileSystem::kTileHeightBottom, BoxCityTileSystem::kTileHeightTop);
					std::uniform_real_distribution<float> size_range(1.f, 1.5f);

					//Tile is getting deactivated or moved
					if (tile.m_activated)
					{
						std::bitset<BoxCityTileSystem::kLocalTileCount * BoxCityTileSystem::kLocalTileCount> bitset(false);
						bitset[tile.m_zone_index] = true;
						
						core::LogInfo("Traffic: Tile Local<%i,%i>, World<%i,%i>, moved", local_tile.i, local_tile.j, world_tile.i, world_tile.j);

						//Move cars
						ecs::Process<GameDatabase, Car, CarMovement, CarSettings, CarTarget, OBBBox, CarGPUIndex, FlagBox, CarBoxListOffset>([&]
						(const auto& instance_iterator, Car& car, CarMovement& car_movement, CarSettings& car_settings, CarTarget& car_target, OBBBox& obb_box_component, CarGPUIndex& car_gpu_index, FlagBox& flag_box, CarBoxListOffset& car_box_index_offset)
							{
								SetupCar(tile, random, begin_tile_x, begin_tile_y, position_range, position_range_z, size_range,
								car, car_movement, car_settings, obb_box_component, car_gpu_index, car_box_index_offset);

								BoxCityCarControl::SetupCarTarget(random, tile_manager, car, car_target, true);

								flag_box.moved = true;

							}, bitset);
					}
					else
					{
						core::LogInfo("Traffic: Tile Local<%i,%i>, World<%i,%i>, created", local_tile.i, local_tile.j, world_tile.i, world_tile.j);

						//Create the indirect instance array in CPU
						//Allocate a lot more, because the cars move
						size_t max_size = render::RoundSizeUp16Bytes(kNumCars * 2 * sizeof(uint32_t)) / sizeof(uint32_t);
						tile.m_instance_list_max_count = static_cast<uint32_t>(max_size);
						std::vector<uint32_t> init_instances_list_data;
						init_instances_list_data.resize(max_size);
						init_instances_list_data[0] = kNumCars;

						//Create cars
						for (size_t i = 0; i < kNumCars; ++i)
						{
							Car car;
							CarMovement car_movement;
							CarSettings car_settings;
							OBBBox obb_box_component;
							CarGPUIndex car_gpu_index;
							CarControl car_control;
							FlagBox flag_box;
							CarBoxListOffset car_box_list_offset;
							//Alloc GPU slot
							car_gpu_index.gpu_slot = static_cast<uint16_t>(tile.m_zone_index * kNumCars + i);

							SetupCar(tile, random, begin_tile_x, begin_tile_y, position_range, position_range_z, size_range,
								car, car_movement, car_settings, obb_box_component, car_gpu_index, car_box_list_offset);

							CarTarget car_target;
							BoxCityCarControl::SetupCarTarget(random, tile_manager, car, car_target, true);

							flag_box.moved = true;

							Instance instance = ecs::AllocInstance<GameDatabase, CarType>(tile.m_zone_index)
								.Init<Car>(car)
								.Init<CarMovement>(car_movement)
								.Init<CarSettings>(car_settings)
								.Init<CarTarget>(car_target)
								.Init<OBBBox>(obb_box_component)
								.Init<CarGPUIndex>(car_gpu_index)
								.Init<CarControl>(car_control)
								.Init<FlagBox>(flag_box)
								.Init<CarBoxListOffset>(car_box_list_offset)
								.Init<LastPositionAndRotation>(LastPositionAndRotation{ car.position.Last(), car.rotation.Last()});

							if (tile.m_zone_index == 0 && i == 0)
							{
								//The first car is the player car
								m_player_car = instance;
							}

							//Fill the instances list with the offset
							init_instances_list_data[1 + i] = static_cast<uint32_t>(m_GPU_memory_render_module->GetStaticGPUMemoryOffset(GetGPUHandle()) + car_gpu_index.gpu_slot * sizeof(GPUBoxInstance));
						}
						//Fill the rest with 0xFFFFFFFF
						for (uint32_t i = static_cast<uint32_t>(init_instances_list_data.size()); i < tile.m_instance_list_max_count; ++i)
							init_instances_list_data[i] = 0xFFFFFFFF;

						//Create the gpu instances list memory
						tile.m_instances_list_handle = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, tile.m_instance_list_max_count * sizeof(uint32_t), init_instances_list_data.data(), render::GetGameFrameIndex(m_render_system));

						//Init tile
						tile.m_activated = true;
					}
				}
			}
		}
	}

	void Manager::AppendVisibleInstanceLists(const helpers::Frustum& frustum, std::vector<uint32_t>& instance_lists_offsets_array)
	{
		for (auto& tile : m_tiles)
		{
			if (helpers::CollisionFrustumVsAABB(frustum, tile.m_bounding_box) && tile.m_instances_list_handle.IsValid())
			{
				size_t instance_list_offset = m_GPU_memory_render_module->GetStaticGPUMemoryOffset(tile.m_instances_list_handle);
				instance_lists_offsets_array.push_back(static_cast<uint32_t>(instance_list_offset));

				COUNTER_INC_VALUE(c_Car_Summitted, static_cast<uint32_t>(ecs::GetNumInstances<GameDatabase, CarType>(tile.m_zone_index)));
			}
		}
	}


	void Manager::UpdateCars(platform::Game* game, job::System* job_system, job::JobAllocator<1024 * 1024>* job_allocator, const helpers::Camera& camera, job::Fence& update_fence, BoxCityTileSystem::Manager* tile_manager, uint32_t frame_index, float elapsed_time)
	{
		std::bitset<BoxCityTileSystem::kLocalTileCount* BoxCityTileSystem::kLocalTileCount> full_bitset(0xFFFFFFFF >> (32 - kLocalTileCount * kLocalTileCount));
		//std::bitset<BoxCityTileSystem::kLocalTileCount* BoxCityTileSystem::kLocalTileCount> camera_bitset = GetCameraBitSet(camera);
		//Update the cars in the direction of the target
		ecs::AddJobs < GameDatabase, Car, CarMovement, CarTarget, CarSettings, CarControl, CarBuildingsCache, OBBBox, CarGPUIndex, FlagBox> (job_system, update_fence, job_allocator, 256,
			[elapsed_time, manager = this, tile_manager = tile_manager, game, camera_position = camera.GetPosition(), frame_index]
			(const auto& instance_iterator, Car& car, CarMovement& car_movement, CarTarget& car_target, CarSettings& car_settings, CarControl& car_control, CarBuildingsCache& car_buildings_cache, OBBBox& obb_box, CarGPUIndex& car_gpu_index, FlagBox& flag_box)
			{
				//Update position
				if (instance_iterator == manager->GetPlayerCar().Get<GameDatabase>() && manager->m_player_control_enable)
				{
					//Update control
					BoxCityCarControl::UpdatePlayerControl(game, car_control, elapsed_time);
				}
				else
				{
					//AI car
					BoxCityCarControl::UpdateAIControl(random_thread_local, instance_iterator.m_instance_index, car_control, car, car_movement, car_settings, car_target, car_buildings_cache, frame_index, elapsed_time, tile_manager, manager,  camera_position);
				}
				
				//Calculate forces
				glm::vec3 linear_forces(0.f, 0.f, 0.f);
				glm::vec3 angular_forces(0.f, 0.f, 0.f);
				glm::vec3 position_offset(0.f, 0.f, 0.f);

				BoxCityCarControl::CalculateControlForces(car, car_movement, car_settings, car_control, elapsed_time, linear_forces, angular_forces);
				BoxCityCarControl::CalculateCollisionForces(tile_manager, camera_position, obb_box, linear_forces, angular_forces, position_offset);

				//Integrate
				BoxCityCarControl::IntegrateCar(car, car_movement, car_settings, linear_forces, angular_forces, position_offset, elapsed_time);

				if (!flag_box.moved)
				{
					//Check if it is outside of the tile (change tile or cycle the car)
					WorldTilePosition current_world_tile = manager->GetTile(instance_iterator.m_zone_index).m_tile_position;
					WorldTilePosition next_world_tile = CalculateWorldPositionToWorldTile(*car.position);

					if (current_world_tile.i != next_world_tile.i || current_world_tile.j != next_world_tile.j)
					{
						LocalTilePosition next_local_tile = CalculateLocalTileIndex(next_world_tile);
						uint32_t next_zone_index = CalculateLocalTileToZoneIndex(next_local_tile);
	
						//Check if we need to cycle front-back or left-right, in that case we need to move the target
						//LocalTilePosition last_local_tile = CalculateLocalTileIndex(current_world_tile);
						//core::LogInfo("Car current world <%d,%d> current local <%d,%d>, next world<%d,%d> next local <%d,%d>", current_world_tile.i, current_world_tile.j, last_local_tile.i, last_local_tile.j, next_world_tile.i, next_world_tile.j, next_local_tile.i, next_local_tile.j);
						//Check it it was a jump
						if (abs(manager->GetTile(next_zone_index).m_tile_position.i != next_world_tile.i) ||
							abs(manager->GetTile(next_zone_index).m_tile_position.j != next_world_tile.j))
						{
							glm::vec3 source_reference(next_world_tile.i * kTileSize, next_world_tile.j * kTileSize, 0.f);
							glm::vec3 dest_reference(manager->GetTile(next_zone_index).m_tile_position.i * kTileSize, manager->GetTile(next_zone_index).m_tile_position.j * kTileSize, 0.f);
							//A jump has happen, recalculate the target
							car_target.target = (car_target.target - source_reference) + dest_reference;
							car_target.last_target = (car_target.last_target - source_reference) + dest_reference;
							car.position.Reset(*car.position - source_reference + dest_reference);
						}

						//Needs to move
						instance_iterator.Move(next_zone_index);

						assert(manager->GetTile(next_zone_index).m_bounding_box.Inside(*car.position, 1.f));
					}
					else
					{
						assert(manager->GetTile(instance_iterator.m_zone_index).m_bounding_box.Inside(*car.position, 1.f));
					}
				}
				flag_box.moved = false;


				//Update OOBB
				obb_box.position = *car.position;
				obb_box.rotation = glm::toMat3(*car.rotation);

			}, full_bitset, &g_profile_marker_Car_Update);
	}

	void Manager::GenerateZoneDescriptors()
	{
		//We create all the tile descriptors
		constexpr int32_t range = static_cast<int32_t>(kLocalTileCount / 2);
		for (int32_t i_offset = -range; i_offset <= range; ++i_offset)
		{
			for (int32_t j_offset = -range; j_offset <= range; ++j_offset)
			{
				TileDescriptor tile_descriptor;
				tile_descriptor.i_offset = i_offset;
				tile_descriptor.j_offset = j_offset;

				//This is a valid descriptor tile
				m_tile_descriptors.push_back(tile_descriptor);
			}
		}
	}

	Manager::Tile& Manager::GetTile(const LocalTilePosition& local_tile)
	{
		return m_tiles[CalculateLocalTileToZoneIndex(local_tile)];
	}

	void Manager::RegisterECSChange(uint32_t zone_index, uint32_t instance_index)
	{
		//We need to invalidate that range of memory
		auto& invalidated_memory_block = m_invalidated_memory_block[zone_index];

		//Calculate the block offset, it is a 16bytes aligned block and starting in 1, as 0 has the size
		uint32_t invalidated_block_offset = static_cast<uint32_t>(render::RoundOffsetDown16Bytes((instance_index + 1) * sizeof(uint32_t)));

		//Check if already has been added
		for (auto& block_offset : invalidated_memory_block)
		{
			if (block_offset == invalidated_block_offset) return;
		}

		//Add the new invalidate block
		invalidated_memory_block.push_back(invalidated_block_offset);

		//Invalidate zone
		InvalidateZone(zone_index);
	}

	void Manager::InvalidateZone(uint32_t zone)
	{
		for (const uint32_t invalidated_zones : m_invalidated_zones)
		{
			if (zone == invalidated_zones) return;
		}

		m_invalidated_zones.push_back(zone);
	}

	void Manager::ProcessCarMoves()
	{
		//Recreate the instance list for the zone
		if (c_traffic_full_instance_list_upload)
		{
			for (const auto& zone_index : m_invalidated_zones)
			{
				Tile& tile = m_tiles[zone_index];
				std::bitset<BoxCityTileSystem::kLocalTileCount* BoxCityTileSystem::kLocalTileCount> zone_bitset(false);
				zone_bitset[zone_index] = true;
				std::vector<uint32_t> updated_instance_list_data; 
				updated_instance_list_data.reserve(tile.m_instance_list_max_count);
				updated_instance_list_data.push_back(0);//Space for the size
				uint32_t base_offset = static_cast<uint32_t>(m_GPU_memory_render_module->GetStaticGPUMemoryOffset(GetGPUHandle()));

				ecs::Process<GameDatabase, const CarGPUIndex>([&](const auto& instance_iterator, const CarGPUIndex& car_gpu_index)
					{
						assert(instance_iterator.m_zone_index == zone_index);
				updated_instance_list_data.push_back(static_cast<uint32_t>(base_offset + car_gpu_index.gpu_slot * sizeof(GPUBoxInstance)));
					}, zone_bitset);

				for (uint32_t i = static_cast<uint32_t>(updated_instance_list_data.size()); i < tile.m_instance_list_max_count; ++i)
					updated_instance_list_data.push_back(0xFFFFFFFF);

				assert(updated_instance_list_data.size() <= tile.m_instance_list_max_count);
				updated_instance_list_data[0] = static_cast<uint32_t>(ecs::GetNumInstances<GameDatabase, CarType>(zone_index));

				//Update the instance list in the GPU
				m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, tile.m_instances_list_handle, updated_instance_list_data.data(), render::RoundSizeUp16Bytes(updated_instance_list_data.size() * sizeof(uint32_t)), render::GetGameFrameIndex(m_render_system));
			}
		}
		else
		{
			for (const auto& zone_index : m_invalidated_zones)
			{
				Tile& tile = m_tiles[zone_index];
				uint32_t num_instances = static_cast<uint32_t>(ecs::GetNumInstances<GameDatabase, CarType>(tile.m_zone_index));

				assert(num_instances < tile.m_instance_list_max_count - 1);

				auto fill_block = [&](uint32_t memory_block_data[4], uint32_t memory_block)
				{
					//Needs the -1 because the memory block is including the first element that is the size

					auto fill_block_element = [&](uint32_t index, uint32_t instance_index)
					{
						if (instance_index < num_instances)
						{
							//It uses the index to calculate the offset
							const CarGPUIndex& gpu_car_index = ecs::GetComponentData<GameDatabase, CarType, CarGPUIndex>(tile.m_zone_index, instance_index);
							assert(gpu_car_index.IsValid());
							memory_block_data[index] = static_cast<uint32_t>(m_GPU_memory_render_module->GetStaticGPUMemoryOffset(GetGPUHandle()) + gpu_car_index.gpu_slot * sizeof(GPUBoxInstance));
						}
						else
						{
							memory_block_data[index] = 0xFFFFFFFF; //That is bad, nobody ever can access here
						}
					};

					if (memory_block == 0)
					{
						//The first one is the count
						memory_block_data[0] = num_instances;
						fill_block_element(1, 0);
						fill_block_element(2, 1);
						fill_block_element(3, 2);
					}
					else
					{
						uint32_t base_instance_index_in_block = (memory_block / 4) - 1;

						fill_block_element(0, base_instance_index_in_block);
						fill_block_element(1, base_instance_index_in_block + 1);
						fill_block_element(2, base_instance_index_in_block + 2);
						fill_block_element(3, base_instance_index_in_block + 3);
					}
				};

				uint32_t updated_memory_block[4];

				for (auto& memory_block : m_invalidated_memory_block[zone_index])
				{
					//Create and update of the block to the GPU
					fill_block(updated_memory_block, memory_block);

					//Send it to the GPU
					m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, tile.m_instances_list_handle, updated_memory_block, 16, render::GetGameFrameIndex(m_render_system), memory_block);
				}


				//Update zero with the size, an invalidation of the zone has produce a change in the size
				fill_block(updated_memory_block, 0);

				//Send it to the GPU
				m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, tile.m_instances_list_handle, updated_memory_block, 16, render::GetGameFrameIndex(m_render_system), 0);
			}
		}

		//Clear for the next frame
		m_invalidated_zones.clear();
		for (size_t i = 0; i < kLocalTileCount * kLocalTileCount; ++i)
		{
			m_invalidated_memory_block[i].clear();
		}
	}

}