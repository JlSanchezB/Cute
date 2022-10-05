#include "box_city_tile_manager.h"
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <render/render.h>
#include <render_module/render_module_gpu_memory.h>
#include <core/profile.h>

namespace BoxCityTileSystem
{
	void Manager::Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module)
	{
		m_device = device;
		m_render_system = render_system;
		m_GPU_memory_render_module = GPU_memory_render_module;

		m_camera_tile_position.i = std::numeric_limits<int32_t>::max();
		m_camera_tile_position.j = std::numeric_limits<int32_t>::max();

		//Build tile descriptors
		GenerateTileDescriptors();

		//Simulate one frame in the center, it has to create all tiles around origin
		Update(glm::vec3(0.f, 0.f, 0.f));
	}

	void Manager::Shutdown()
	{
		//Unload each tile
		for (auto& tile : m_tiles)
		{
			if (tile.IsVisible())
			{
				tile.DespawnTile(this);
			}
		}
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

		constexpr float fudge_top_range = 10.0f;

		if (m_camera_top_range && camera_position.z < (kTileHeightTopViewRange - fudge_top_range))
			camera_moved = true;
		if (!m_camera_top_range && camera_position.z > (kTileHeightTopViewRange + fudge_top_range))
			camera_moved = true;

		if (camera_moved || m_pending_streaming_work)
		{
			if (camera_moved)
			{
				m_camera_tile_position = WorldTilePosition{ static_cast<int32_t>(-0.5f + (camera_position.x / kTileSize)), static_cast<int32_t>(-0.5f + (camera_position.y / kTileSize)) };
				m_camera_top_range = (camera_position.z > kTileHeightTopViewRange);
			}

			uint32_t num_tile_changed = 0;
			const uint32_t max_tile_changed_per_frame = 1;

			//We go through all the tiles and check if they are ok in the current state or they need update
			//The tile descriptors are in order from the center
			for (auto& tile_descriptor : m_tile_descriptors)
			{
				//Calculate world tile
				WorldTilePosition world_tile{ m_camera_tile_position.i + tile_descriptor.i_offset, m_camera_tile_position.j + tile_descriptor.j_offset};
				
				//Calculate local tile
				LocalTilePosition local_tile = CalculateLocalTileIndex(world_tile);
				Tile& tile = GetTile(local_tile);

				//Depends of the camera, we calculate the lod 1 or 0
				uint32_t lod = tile_descriptor.lod;
				bool m_visible = tile_descriptor.m_loaded;

				//If we are under the horizon, we don't need LOD2 or LOD1
				if (camera_position.z < kTileHeightTopViewRange && lod > 0)
					m_visible = false;
			

				//Check if the tile has the different world index
				if ((tile.GetWorldTilePosition().i != world_tile.i || tile.GetWorldTilePosition().j != world_tile.j) && tile.IsVisible() || !m_visible && tile.IsVisible())
				{
					tile.DespawnTile(this);
					num_tile_changed++;

					core::LogInfo("Tile Local<%i,%i>, World<%i,%i>, unvisible", local_tile.i, local_tile.j, world_tile.i, world_tile.j);
				}

				//If tile is unloaded but it needs to be m_loaded
				if (!tile.IsVisible() && m_visible && num_tile_changed < max_tile_changed_per_frame)
				{
					//Create new time
					tile.BuildTileData(this, local_tile, world_tile);
					tile.SpawnTile(this, lod);
					num_tile_changed++;

					core::LogInfo("Tile Local<%i,%i>, World<%i,%i>, Lod<%i> visible", local_tile.i, local_tile.j, world_tile.i, world_tile.j, lod);
				}

				if (tile.IsVisible() && tile.CurrentLod() != lod && num_tile_changed < max_tile_changed_per_frame)
				{
					//Change lod
					tile.LodTile(this, lod);
					num_tile_changed++;
				}
			}

			if (num_tile_changed < max_tile_changed_per_frame)
			{
				//All done
				m_pending_streaming_work = false;
			}
			else
			{
				//It needs to go back
				m_pending_streaming_work = true;
			}
		}
	}

	render::AllocHandle& Manager::GetGPUHandle(uint32_t zoneID, uint32_t lod_group)
	{
		return m_tiles[zoneID].GetLodGPUAllocation(static_cast<LODGroup>(lod_group));
	}

	void Manager::GenerateTileDescriptors()
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
				tile_descriptor.distance = sqrtf(static_cast<float>(i_offset) * static_cast<float>(i_offset) + static_cast<float>(j_offset) * static_cast<float>(j_offset));

				//Calculate normal distance
				tile_descriptor.normalized_distance = tile_descriptor.distance / static_cast<float>(range);

				//You want a tile only inside the radius
				if (tile_descriptor.normalized_distance <= 1.2f)
				{
					tile_descriptor.m_loaded = true;
					//Calculate LODs
					if (tile_descriptor.normalized_distance < 0.75f)
					{
						//0.0 to 0.75 is max lod, lod 0
						tile_descriptor.lod = 0;
					}
					else if (tile_descriptor.normalized_distance < 0.90f)
					{
						//0.75 to 0.9 is lod 1
						tile_descriptor.lod = 1;
					}
					else
					{
						//0.9 to 1.0 is lod 2
						tile_descriptor.lod = 2;
					}
				}
				else
				{
					//Non m_loaded
					tile_descriptor.m_loaded = false;
					tile_descriptor.lod = -1;
				}

				tile_descriptor.index = static_cast<uint32_t>(m_tile_descriptors.size());

				//This is a valid descriptor tile
				m_tile_descriptors.push_back(tile_descriptor);
			}
		}

		//Sort tiles from center to far
		std::sort(m_tile_descriptors.begin(), m_tile_descriptors.end(), [](const TileDescriptor& a, const TileDescriptor& b)
			{
				return a.normalized_distance < b.normalized_distance;
			});
	}

	Tile& Manager::GetTile(const LocalTilePosition& local_tile)
	{
		return m_tiles[local_tile.i + local_tile.j * kLocalTileCount];
	}
}
