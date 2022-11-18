#include "box_city_traffic_manager.h"
#include <core/profile.h>
#include "box_city_tile_manager.h"

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

		//Create traffic around camera position
		Update(camera_position);
	}

	void Manager::Shutdown()
	{

	}

	void Manager::Update(const glm::vec3& camera_position)
	{
		PROFILE_SCOPE("BoxCityTileManager", 0xFFFF77FF, "Update");
		//Check if the camera is still in the same tile, using some fugde factor
		constexpr float fudge_factor = 0.05f;
		float min_x = (-0.5f + m_camera_tile_position.i - fudge_factor) * kTileSize;
		float min_y = (-0.5f + m_camera_tile_position.j - fudge_factor) * kTileSize;
		float max_x = (-0.5f + m_camera_tile_position.i + 1 + fudge_factor) * kTileSize;
		float max_y = (-0.5f + m_camera_tile_position.j + 1 + fudge_factor) * kTileSize;

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
					/*
					//Tile is getting deactivated or moved
					if (tile_descriptor.active && tile.m_activated)
					{
						std::bitset<kLocalTileCount* kLocalTileCount> bitset(false);
						bitset[tile_descriptor.index] = true;

						//Move cars
						ecs::Process<GameDatabase, Car>([](const auto& instance_iterator, Car& car)
							{
								

							}, bitset);
					}
					else if (tile_descriptor.active && !tile.m_activated)
					{
						//Create cars
						Instance instance = ecs::AllocInstance<GameDatabase, CarType>(tile_descriptor.index).
							Init<>();
					}
					else //tile descriptor is not active
					{
						//Destroy cars in this tile/zone
						std::bitset<kLocalTileCount* kLocalTileCount> bitset(false);
						bitset[tile_descriptor.index] = true;

						ecs::Process<GameDatabase, Car>([](const auto& instance_iterator, Car& car)
							{
								instance_iterator.Dealloc();
							}, bitset);
					}
					*/
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
}