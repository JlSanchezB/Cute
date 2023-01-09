#include "box_city_tile_manager.h"
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <render/render.h>
#include <render_module/render_module_gpu_memory.h>
#include <core/profile.h>
#include <core/control_variables.h>
#include "box_city_descriptors.h"

CONTROL_VARIABLE_BOOL(c_use_loading_thread, true, "BoxCityTileManager", "Use loading thread for loading");

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

		//Generate zone descriptors
		GenerateZoneDescriptors();

		//Create loading thread
		m_loading_thread.reset( new core::Thread(L"TileLoading Thread", core::ThreadPriority::Background, &LoadingThreadRun, this));

		//Simulate one frame in the center, it has to create all tiles around origin
		Update(glm::vec3(0.f, 0.f, 0.f));

	}

	void Manager::Shutdown()
	{
		//Quit the loading thread
		{
			std::unique_lock<core::Mutex> lock_guard(m_loading_access_mutex);

			//Mark the loading thread for quit
			m_loading_thread_quit = true;

			//Notify the loading thread that has work
			m_loading_queue_condition_variable.notify_one();
		}

		//Join the thread
		m_loading_thread->join();

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
		float min_x = (m_camera_tile_position.i - fudge_factor) * kTileSize;
		float min_y = (m_camera_tile_position.j - fudge_factor) * kTileSize;
		float max_x = (m_camera_tile_position.i + 1 + fudge_factor) * kTileSize;
		float max_y = (m_camera_tile_position.j + 1 + fudge_factor) * kTileSize;

		//If the camera has move of tile, destroy the tiles out of view and create the new ones
		bool camera_moved = (camera_position.x < min_x) || (camera_position.y < min_y) || (camera_position.x > max_x) || (camera_position.y > max_y);

		constexpr float fudge_top_range = 10.0f;

		if (m_camera_top_range && camera_position.z < (kTileHeightTopViewRange - fudge_top_range))
			camera_moved = true;
		if (!m_camera_top_range && camera_position.z > (kTileHeightTopViewRange + fudge_top_range))
			camera_moved = true;

		bool new_tiles_loaded = m_tiles_loaded.exchange(false);

		if (camera_moved || m_pending_streaming_work || new_tiles_loaded)
		{
			if (camera_moved)
			{
				m_camera_tile_position = CalculateWorldPositionToWorldTile(camera_position);
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
				bool tile_needs_to_be_visible = tile_descriptor.m_loaded;

				//If we are under the horizon, we don't need LOD2 or LOD1
				if (camera_position.z < kTileHeightTopViewRange && lod > 0)
					tile_needs_to_be_visible = false;
			

				//Check if the tile has the different world index
				if ((tile.GetWorldTilePosition().i != world_tile.i || tile.GetWorldTilePosition().j != world_tile.j) && tile.IsVisible() || !tile_needs_to_be_visible && tile.IsVisible())
				{
					tile.DespawnTile(this);
					num_tile_changed++;

					core::LogInfo("Tile Local<%i,%i>, World<%i,%i>, unvisible", local_tile.i, local_tile.j, world_tile.i, world_tile.j);
				}

				//If tile is unloaded or changed world position but it needs to be m_loaded, added to the queue
				if ((tile.GetWorldTilePosition().i != world_tile.i || tile.GetWorldTilePosition().j != world_tile.j) && tile_needs_to_be_visible || !tile.IsLoaded() && tile_needs_to_be_visible)
				{
					if (!tile.IsLoading())
					{
						AddTileToLoad(tile, local_tile, world_tile);
					}
				}

				//The tile is loaded but still not visible
				if (!tile.IsVisible() && tile.IsLoaded() && tile_needs_to_be_visible && num_tile_changed < max_tile_changed_per_frame)
				{
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

	bool Manager::GetNextTrafficTarget(std::mt19937& random, const glm::vec3& position, glm::vec3& next_target) const
	{
		//Calculate the current tile
		int32_t offset_i = static_cast<int32_t>(floor(2.f * (position.x / kTileSize)));
		int32_t offset_j = static_cast<int32_t>(floor(2.f * (position.y / kTileSize)));
		int32_t offset_k = static_cast<int32_t>(floor(4.f * (position.z - kTileHeightBottom) / (kTileHeightTop - kTileHeightBottom)));
		offset_k = glm::clamp<int32_t>(offset_k, 0, 3);


		//Add the offset
		switch (random() % 6)
		{
		case 0:
			offset_i++;
			break;
		case 1:
			offset_j++;
			break;
		case 2:
			offset_k++;
			break;
		case 3:
			offset_i--;
			break;
		case 4:
			offset_j--;
			break;
		case 5:
			offset_k--;
			break;
		}
		offset_k = glm::clamp<int32_t>(offset_k, 0, 3);

		//Calculate the current target inside the tile
		WorldTilePosition world_tile_position{ (offset_i < 0) ? (offset_i - 1) / 2: offset_i / 2, (offset_j < 0) ? (offset_j - 1) / 2: offset_j / 2 };

		//Get tile
		const Tile& tile = GetTile(CalculateLocalTileIndex(world_tile_position));
		
		if (!tile.IsLoaded()) return false;

		//Get offset inside the tile
		uint32_t x = static_cast<uint32_t>((offset_i + 1000000) % 2);
		uint32_t y = static_cast<uint32_t>((offset_j + 1000000) % 2);
		uint32_t z = static_cast<uint32_t>(offset_k);
		
		//Get the position
		next_target = tile.GetTrafficTargetPosition(x, y, z);

		assert(glm::distance(glm::vec2(next_target), glm::vec2(position)) < 2000.f);
		return true;
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

	const Tile& Manager::GetTile(const LocalTilePosition& local_tile) const
	{
		return m_tiles[local_tile.i + local_tile.j * kLocalTileCount];
	}

	void Manager::GenerateZoneDescriptors()
	{
		std::mt19937 random(0);

		m_descriptor_zones.resize(kNumZonesZ * kNumZonesXY * kNumZonesXY);

		//Get random post inside the zone
		for (size_t i = 0; i < kNumZonesXY; i++)
		{
			for (size_t j = 0; j < kNumZonesXY; j++)
			{
				for (size_t k = 0; k < kNumZonesZ; k++)
				{
					auto& zone = m_descriptor_zones[k + kNumZonesZ * (i + j * kNumZonesXY)];

					zone.descriptor_index = random() % kNumZoneDescriptors;
					
					std::uniform_real_distribution<float> position_x((static_cast<float>(i) + 0.1f) * kZoneWorldSizeXY / kNumZonesXY, (static_cast<float>(i) - 0.1f + 1) * kZoneWorldSizeXY / kNumZonesXY);
					std::uniform_real_distribution<float> position_y((static_cast<float>(j) + 0.1f) * kZoneWorldSizeXY / kNumZonesXY, (static_cast<float>(j) - 0.1f + 1) * kZoneWorldSizeXY / kNumZonesXY);
					std::uniform_real_distribution<float> position_z(kTileHeightBottom + (static_cast<float>(k) + 0.1f) * kZoneWorldSizeZ / kNumZonesZ, kTileHeightBottom + (static_cast<float>(k) - 0.1f + 1) * kZoneWorldSizeZ / kNumZonesZ);
					
					zone.position.x = position_x(random);
					zone.position.y = position_y(random);
					zone.position.z = position_z(random);
				}
			}
		}

	}

	std::optional<uint32_t> Manager::GetZoneDescriptorIndex(const glm::vec3& position)
	{
		glm::vec3 adjusted_position;
		adjusted_position.x = fmodf(position.x + 1000.f * kZoneWorldSizeXY, kZoneWorldSizeXY);
		adjusted_position.y = fmodf(position.y + 1000.f * kZoneWorldSizeXY, kZoneWorldSizeXY);
		adjusted_position.z = position.z;
		//Get the current box
		int32_t i = static_cast<uint32_t>(kNumZonesXY * adjusted_position.x / kZoneWorldSizeXY);
		int32_t j = static_cast<uint32_t>(kNumZonesXY * adjusted_position.y / kZoneWorldSizeXY);
		int32_t k = static_cast<uint32_t>(kNumZonesZ * glm::clamp(adjusted_position.z - kTileHeightBottom, 0.f, kZoneWorldSizeZ) / kZoneWorldSizeZ);
		
		struct SortData
		{
			uint32_t descriptor_index;
			float distance;
		};
		std::array<SortData, 3 * 3 * 3> sort_data;
		size_t sort_index = 0;
		//Calculate the two closes zones
		for (int32_t offset_i = i - 1; offset_i <= i + 1; ++offset_i)
		{
			for (int32_t offset_j = j - 1; offset_j <= j + 1; ++offset_j)
			{
				for (int32_t offset_k = k - 1; offset_k <= k + 1; ++offset_k)
				{
					int32_t box_i = (offset_i + kNumZonesXY) % kNumZonesXY;
					int32_t box_j = (offset_j + kNumZonesXY) % kNumZonesXY;

					if (offset_k < 0 || offset_k > kNumZonesZ)
						continue;

					int32_t box_k = (offset_k + kNumZonesZ) % kNumZonesZ;

					auto& zone = m_descriptor_zones[box_k + kNumZonesZ * (box_i + box_j * kNumZonesXY)];

					sort_data[sort_index].descriptor_index = zone.descriptor_index;

					//We need to force a same ratio between XY and Z
					glm::vec3 distance_vector = adjusted_position - zone.position;
					distance_vector.z *= kZoneXYvsZRatio;
					sort_data[sort_index].distance = glm::length(distance_vector);
					sort_index++;
				}
			}
		}
		
		//Partial sort only 2 
		std::partial_sort(sort_data.begin(), sort_data.begin() + 2, sort_data.begin() + sort_index,
			[](const SortData& a, const SortData& b)
			{
				return a.distance < b.distance;
			});

		//Calculate if it is a gap
		if (glm::abs(sort_data[0].distance - sort_data[1].distance) <= (kZoneDescriptors[sort_data[0].descriptor_index].corridor_size + kZoneDescriptors[sort_data[1].descriptor_index].corridor_size))
		{
			//It is a gap, is not going to work
			return {};
		}
		else
		{
			//Return the closest one
			return sort_data[0].descriptor_index;
		}
	}
	void Manager::AddTileToLoad(Tile& tile, const LocalTilePosition& local_tile_position, const WorldTilePosition& world_tile_position)
	{
		assert(!tile.IsVisible());
		assert(!tile.IsLoading());

		if (c_use_loading_thread)
		{
			tile.AddedToLoadingQueue();

			std::unique_lock<core::Mutex> lock_guard(m_loading_access_mutex);

			//queue tile
			m_loading_queue.push(LoadingJob{ &tile, local_tile_position, world_tile_position });

			//Notify the loading thread that has work
			m_loading_queue_condition_variable.notify_one();
		}
		else
		{
			//Just load it and mark for process
			tile.BuildTileData(this, local_tile_position, world_tile_position);
			m_tiles_loaded.exchange(true);
		}
	}

	void Manager::LoadingThreadRun(Manager* manager)
	{
		//Set name to the profiler
		core::OnThreadCreate("TileLoading Thread");

		std::unique_lock<core::Mutex> lock_guard(manager->m_loading_access_mutex);

		while (!manager->m_loading_thread_quit)
		{
			manager->m_loading_queue_condition_variable.wait(lock_guard, [manager]()
				{
					return !manager->m_loading_queue.empty() || manager->m_loading_thread_quit;
				});

			if (!manager->m_loading_queue.empty())
			{
				LoadingJob job = manager->m_loading_queue.front();
				manager->m_loading_queue.pop();

				lock_guard.unlock();

				//Load the tile
				{
					assert(job.tile->IsLoading());
					job.tile->BuildTileData(manager, job.local_tile_position, job.world_tile_position);
				}

				//Indicate to the tile manager that tiles have been loaded
				manager->m_tiles_loaded.exchange(true);

				lock_guard.lock();
			}
		}
	}
}
