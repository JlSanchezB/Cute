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

PROFILE_DEFINE_MARKER(g_profile_marker_Car_Update, "Main", 0xFFFFAAAA, "CarUpdate");

//List of control variables
CONTROL_VARIABLE(float, c_car_target_range, 1.f, 10000.f, 400.f, "Traffic", "Car target range");
CONTROL_VARIABLE(float, c_car_velocity, 0.f, 1000.f, 24.f, "Traffic", "Car velocity");


namespace BoxCityTrafficSystem
{
	void Manager::Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module, const glm::vec3& camera_position)
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

		//We create all the free slots for the GPU
		m_free_gpu_slots.resize(kNumCars * kLocalTileCount * kLocalTileCount);
		for (size_t i = 0; i <kNumCars * kLocalTileCount * kLocalTileCount; ++i)
		{
			m_free_gpu_slots[i] = static_cast<uint16_t>(i);
		}

		//Create traffic around camera position
		Update(camera_position);	
	}

	void Manager::Shutdown()
	{
		//Deallocate GPU memory
		m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, m_gpu_memory, render::GetGameFrameIndex(m_render_system));
		m_free_gpu_slots.clear();
	}

	void Manager::SetupCar(Tile& tile, std::mt19937& random, float begin_tile_x, float begin_tile_y,
		std::uniform_real_distribution<float>& position_range, std::uniform_real_distribution<float>& position_range_z, std::uniform_real_distribution<float>& size_range,
		Car& car, CarSettings& car_settings, OBBBox& obb_component, AABBBox& aabb_component, CarGPUIndex& car_gpu_index)
	{
		glm::vec3 position(begin_tile_x + position_range(random), begin_tile_y + position_range(random), position_range_z(random));
		float size = size_range(random);
		helpers::OBB obb_box;
		obb_box.position = position;
		obb_box.extents = glm::vec3(size, size, size);
		obb_box.rotation = glm::mat3x3(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);

		helpers::AABB aabb_box;
		helpers::CalculateAABBFromOBB(aabb_box, obb_box);

		car.position = position;
		car_settings.size = glm::vec3(size, size, size);
		obb_component = obb_box;
		aabb_component = aabb_box;

		//Update GPU
		BoxRender box_render;
		box_render.colour = glm::vec4(3.f, 3.f, 3.f, 0.f);

		GPUBoxInstance gpu_box_instance;
		gpu_box_instance.Fill(obb_box);
		gpu_box_instance.Fill(box_render);

		//Update the GPU memory
		m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, m_gpu_memory, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system), car_gpu_index.gpu_slot * sizeof(GPUBoxInstance));
	}

	void Manager::SetupCarTarget(std::mt19937& random, Car& car, CarTarget& car_target)
	{
		//Calculate a new position as target
		std::uniform_real_distribution<float> position_range(-c_car_target_range, c_car_target_range);
		std::uniform_real_distribution<float> position_range_z(BoxCityTileSystem::kTileHeightBottom, BoxCityTileSystem::kTileHeightTop);

		car_target.target = glm::vec3(car.position.x + position_range(random), car.position.y + position_range(random), position_range_z(random));
	}

	void Manager::Update(const glm::vec3& camera_position)
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
					
					//Update tile
					tile.m_tile_position = world_tile;
					tile.m_bounding_box.min = glm::vec3(begin_tile_x, begin_tile_y, BoxCityTileSystem::kTileHeightBottom);
					tile.m_bounding_box.max = glm::vec3(begin_tile_x + kTileSize, begin_tile_y + kTileSize, BoxCityTileSystem::kTileHeightTop);
					
					std::mt19937 random(static_cast<uint32_t>((100000 + world_tile.i) + (100000 + world_tile.j) * kLocalTileCount));

					std::uniform_real_distribution<float> position_range(0.f, kTileSize);
					std::uniform_real_distribution<float> position_range_z(BoxCityTileSystem::kTileHeightBottom, BoxCityTileSystem::kTileHeightTop);
					std::uniform_real_distribution<float> size_range(1.f, 2.f);

					//Tile is getting deactivated or moved
					if (tile.m_activated)
					{
						std::bitset<BoxCityTileSystem::kLocalTileCount * BoxCityTileSystem::kLocalTileCount> bitset(false);
						bitset[tile.m_zone_index] = true;
						
						core::LogInfo("Traffic: Tile Local<%i,%i>, World<%i,%i>, moved", local_tile.i, local_tile.j, world_tile.i, world_tile.j);

						//Move cars
						ecs::Process<GameDatabase, Car, CarSettings, CarTarget, OBBBox, AABBBox, CarGPUIndex>([&](const auto& instance_iterator, Car& car, CarSettings& car_settings, CarTarget& car_target, OBBBox& obb_box_component, AABBBox& aabb_box_component, CarGPUIndex& car_gpu_index)
							{
								SetupCar(tile, random, begin_tile_x, begin_tile_y, position_range, position_range_z, size_range,
								car, car_settings, obb_box_component, aabb_box_component, car_gpu_index);	

								SetupCarTarget(random, car, car_target);
							}, bitset);
					}
					else if (!tile.m_activated)
					{
						core::LogInfo("Traffic: Tile Local<%i,%i>, World<%i,%i>, created", local_tile.i, local_tile.j, world_tile.i, world_tile.j);

						//Create cars
						for (size_t i = 0; i < kNumCars; ++i)
						{
							Car car;
							CarSettings car_settings;
							OBBBox obb_box_component;
							AABBBox aabb_box_component;
							CarGPUIndex car_gpu_index;

							//Alloc GPU slot
							car_gpu_index.gpu_slot = AllocGPUSlot();

							SetupCar(tile, random, begin_tile_x, begin_tile_y, position_range, position_range_z, size_range,
								car, car_settings, obb_box_component, aabb_box_component, car_gpu_index);
							
							CarTarget car_target;
							SetupCarTarget(random, car, car_target);

							Instance instance = ecs::AllocInstance<GameDatabase, CarType>(tile.m_zone_index)
								.Init<Car>(car)
								.Init<CarSettings>(car_settings)
								.Init<CarTarget>(car_target)
								.Init<OBBBox>(obb_box_component)
								.Init<AABBBox>(aabb_box_component)
								.Init<CarGPUIndex>(car_gpu_index);
						}

						//Init tile
						tile.m_activated = true;
					}
				}
			}
		}
	}
	
	thread_local std::mt19937 random_thread_local (std::random_device{}());

	void Manager::UpdateCars(job::System* job_system, job::JobAllocator<1024 * 1024>* job_allocator, const helpers::Camera& camera, job::Fence& update_fence, float elapsed_time)
	{
		std::bitset<BoxCityTileSystem::kLocalTileCount* BoxCityTileSystem::kLocalTileCount> full_bitset(0xFFFFFFFF >> (32 - kLocalTileCount * kLocalTileCount));

		std::bitset<BoxCityTileSystem::kLocalTileCount* BoxCityTileSystem::kLocalTileCount> camera_bitset = GetCameraBitSet(camera);
		//Update the cars in the direction of the target
		ecs::AddJobs<GameDatabase, Car, CarTarget, CarSettings, OBBBox, AABBBox, CarGPUIndex>(job_system, update_fence, job_allocator, 256,
			[elapsed_time, camera_bitset, manager = this](const auto& instance_iterator, Car& car, CarTarget& car_target, CarSettings& car_settings, OBBBox& obb_box, AABBBox& aabb_box, CarGPUIndex& car_gpu_index)
			{
				//Update position
				glm::vec3 direction = glm::normalize(car_target.target - car.position);

				car.position = car.position + direction * c_car_velocity * elapsed_time;
				
				//Check if it is outside of the tile (change tile or cycle the car)

				//Calculate if it needs retargetting
				if (glm::length2(car.position - car_target.target) < 10.f)
				{
					//Retarget
					manager->SetupCarTarget(random_thread_local, car, car_target);
				}

				//Update OOBB and AABB
				obb_box.position = car.position;
				helpers::CalculateAABBFromOBB(aabb_box, obb_box);

				//Update GPU if it is in the camera
				if (camera_bitset[instance_iterator.m_zone_index])
				{
					GPUBoxInstance gpu_box_instance;
					gpu_box_instance.Fill(obb_box);

					//Only the first 3 float4
					manager->m_GPU_memory_render_module->UpdateStaticGPUMemory(manager->m_device, manager->m_gpu_memory, &gpu_box_instance, 3 * sizeof(glm::vec4), render::GetGameFrameIndex(manager->m_render_system), car_gpu_index.gpu_slot * sizeof(GPUBoxInstance));
				}

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
		return m_tiles[local_tile.i + local_tile.j * kLocalTileCount];
	}
	uint16_t Manager::AllocGPUSlot()
	{
		assert(!m_free_gpu_slots.empty());
		uint16_t slot = m_free_gpu_slots.back();
		m_free_gpu_slots.pop_back();
		return slot;
	}
	void Manager::DeallocGPUSlot(uint16_t slot)
	{
		m_free_gpu_slots.push_back(slot);
	}
}