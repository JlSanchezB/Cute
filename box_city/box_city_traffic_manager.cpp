#include "box_city_traffic_manager.h"
#include <core/profile.h>
#include "box_city_tile_manager.h"
#include <render/render.h>

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

		//Allocate GPU memory, num tiles * max num cars
		m_gpu_memory = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, 2 * kNumCars * kLocalTileCount * kLocalTileCount * sizeof(GPUBoxInstance), nullptr, render::GetGameFrameIndex(m_render_system));

		//We create all the free slots for the GPU
		m_free_gpu_slots.resize(2 * kNumCars * kLocalTileCount * kLocalTileCount);
		for (size_t i = 0; i < 2 * kNumCars * kLocalTileCount * kLocalTileCount; ++i)
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

	void Manager::Update(const glm::vec3& camera_position)
	{
		PROFILE_SCOPE("BoxCityTrafficManager", 0xFFFF77FF, "Update");
		//Check if the camera is still in the same tile, using some fugde factor
		constexpr float fudge_factor = 0.05f;
		float min_x = (0.5f + m_camera_tile_position.i - fudge_factor) * kTileSize;
		float min_y = (0.5f + m_camera_tile_position.j - fudge_factor) * kTileSize;
		float max_x = (0.5f + m_camera_tile_position.i + 1 + fudge_factor) * kTileSize;
		float max_y = (0.5f + m_camera_tile_position.j + 1 + fudge_factor) * kTileSize;

		//If the camera has move of tile, destroy the tiles out of view and create the new ones
		bool camera_moved = (camera_position.x < min_x) || (camera_position.y < min_y) || (camera_position.x > max_x) || (camera_position.y > max_y);

		if (camera_moved)
		{
			//Update the current camera tile
			m_camera_tile_position = WorldTilePosition{ static_cast<int32_t>(-0.5f + (camera_position.x / kTileSize)), static_cast<int32_t>(-0.5f + (camera_position.y / kTileSize)) };
			
			//Check if we need to create/destroy traffic
			for (auto& tile_descriptor : m_tile_descriptors)
			{
				//Calculate world tile
				WorldTilePosition world_tile{ m_camera_tile_position.i + tile_descriptor.i_offset, m_camera_tile_position.j + tile_descriptor.j_offset };

				//Calculate local tile
				LocalTilePosition local_tile = CalculateLocalTileIndex(world_tile);
				Tile& tile = GetTile(local_tile);

				//Check if the tile has the different world index
				if ((tile.m_tile_position.i != world_tile.i || tile.m_tile_position.j != world_tile.j))
				{
					//Tile positions
					const float begin_tile_x = world_tile.i * kTileSize;
					const float begin_tile_y = world_tile.j * kTileSize;
					
					//Update tile
					tile.m_tile_position = world_tile;
					tile.m_zone_index = tile_descriptor.index;
					tile.m_bounding_box.min = glm::vec3(begin_tile_x, begin_tile_y, BoxCityTileSystem::kTileHeightBottom);
					tile.m_bounding_box.max = glm::vec3(begin_tile_x + kTileSize, begin_tile_y + kTileSize, BoxCityTileSystem::kTileHeightTop);
					
					std::mt19937 random(static_cast<uint32_t>((100000 + world_tile.i) + (100000 + world_tile.j) * kLocalTileCount));

					std::uniform_real_distribution<float> position_range(0, kTileSize);
					std::uniform_real_distribution<float> position_range_z(BoxCityTileSystem::kTileHeightBottom, BoxCityTileSystem::kTileHeightTop);
					std::uniform_real_distribution<float> size_range(1.f, 2.f);

					//Tile is getting deactivated or moved
					if (tile_descriptor.active && tile.m_activated)
					{
						std::bitset<BoxCityTileSystem::kLocalTileCount * BoxCityTileSystem::kLocalTileCount> bitset(false);
						bitset[tile_descriptor.index] = true;

						core::LogInfo("Traffic: Tile Local<%i,%i>, World<%i,%i>, moved", local_tile.i, local_tile.j, world_tile.i, world_tile.j);

						//Move cars
						ecs::Process<GameDatabase, Car, CarSettings, OBBBox, AABBBox, CarGPUIndex>([&](const auto& instance_iterator, Car& car, CarSettings& car_settings, OBBBox& obb_box_component, AABBBox& aabb_box_component, CarGPUIndex& car_gpu_index)
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
								obb_box_component = obb_box;
								aabb_box_component = aabb_box;

								//Update GPU
								BoxRender box_render;
								box_render.colour = glm::vec4(3.f, 3.f, 3.f, 0.f);

								GPUBoxInstance gpu_box_instance;
								gpu_box_instance.Fill(obb_box);
								gpu_box_instance.Fill(box_render);

								//Update the GPU memory
								m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, m_gpu_memory, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system), car_gpu_index.gpu_slot * sizeof(GPUBoxInstance));

							}, bitset);
					}
					else if (tile_descriptor.active && !tile.m_activated)
					{
						core::LogInfo("Traffic: Tile Local<%i,%i>, World<%i,%i>, created", local_tile.i, local_tile.j, world_tile.i, world_tile.j);

						//Create cars
						for (size_t i = 0; i < kNumCars; ++i)
						{
							glm::vec3 position(begin_tile_x + position_range(random), begin_tile_y + position_range(random), position_range_z(random));
							float size = size_range(random);
							helpers::OBB obb_box;
							obb_box.position = position;
							obb_box.extents = glm::vec3(size, size, size);
							obb_box.rotation = glm::mat3x3(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);

							helpers::AABB aabb_box;
							helpers::CalculateAABBFromOBB(aabb_box, obb_box);

							//Setup GPU
							uint16_t gpu_slot = AllocGPUSlot();

							//GPU memory
							BoxRender box_render;
							box_render.colour = glm::vec4(3.f, 3.f, 3.f, 0.f);

							GPUBoxInstance gpu_box_instance;
							gpu_box_instance.Fill(obb_box);
							gpu_box_instance.Fill(box_render);

							//Update the GPU memory
							m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, m_gpu_memory, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system), gpu_slot * sizeof(GPUBoxInstance));

							Instance instance = ecs::AllocInstance<GameDatabase, CarType>(tile_descriptor.index)
								.Init<Car>(position, glm::quat())
								.Init<CarMovement>(glm::vec3(), glm::quat())
								.Init<CarSettings>(glm::vec3(size, size, size))
								.Init<CarTarget>(glm::vec3(), 0.0)
								.Init<OBBBox>(obb_box)
								.Init<AABBBox>(aabb_box)
								.Init<CarGPUIndex>(gpu_slot);
						}

						tile.m_activated = true;
					}
					else if (!tile_descriptor.active && tile.m_activated)
					{
						core::LogInfo("Traffic: Tile Local<%i,%i>, World<%i,%i>, destroyed", local_tile.i, local_tile.j, world_tile.i, world_tile.j);
						//Destroy cars in this tile/zone
						std::bitset<BoxCityTileSystem::kLocalTileCount* BoxCityTileSystem::kLocalTileCount> bitset(false);
						bitset[tile_descriptor.index] = true;

						ecs::Process<GameDatabase, Car, CarGPUIndex>([manager = this](const auto& instance_iterator, Car& car, CarGPUIndex& car_gpu_index)
							{
								//Release GPU
								manager->DeallocGPUSlot(car_gpu_index.gpu_slot);

								//Release instance
								instance_iterator.Dealloc();
							}, bitset);

						tile.m_activated = false;
					}
				}
			}
		}
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

				//Calculate the distance to the center
				float distance = sqrtf(static_cast<float>(i_offset) * static_cast<float>(i_offset) + static_cast<float>(j_offset) * static_cast<float>(j_offset));

				//Calculate normal distance
				float normalized_distance = distance / static_cast<float>(range);

				//You want a tile only inside the radius
				if (normalized_distance <= 1.2f)
				{
					tile_descriptor.active = true;
				}
				else
				{
					tile_descriptor.active = false;
				}

				tile_descriptor.index = static_cast<uint32_t>(m_tile_descriptors.size());

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