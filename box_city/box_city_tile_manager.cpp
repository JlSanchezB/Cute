#include "box_city_tile_manager.h"
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <render/render.h>
#include <render_module/render_module_gpu_memory.h>
#include <core/profile.h>
#include <core/control_variables.h>
#include "box_city_descriptors.h"
#include <job/job.h>

CONTROL_VARIABLE_BOOL(c_use_loading_thread, true, "BoxCityTileManager", "Use loading thread for loading");

namespace
{
	bool CollisionPanelVsPanel(const glm::vec2& position_a, const glm::vec2& size_a, const glm::vec2& position_b, const glm::vec2& size_b)
	{
		glm::vec2 min_a = position_a - size_a;
		glm::vec2 max_a = position_a + size_a;
		glm::vec2 min_b = position_b - size_b;
		glm::vec2 max_b = position_b + size_b;
		// Exit with no intersection if separated along an axis
		if (max_a.x < min_b.x || min_a.x > max_b.x) return false;
		if (max_a.y < min_b.y || min_a.y > max_b.y) return false;
		// Overlapping
		return true;
	}
}

namespace BoxCityTileSystem
{
	void Manager::AppendVisibleInstanceLists(const helpers::Frustum& frustum, std::vector<uint32_t>& instance_lists_offsets_array)
	{
		for (auto& tile : m_tiles)
		{
			if (tile.IsVisible() && helpers::CollisionFrustumVsAABB(frustum, tile.GetBoundingBox()))
			{
				//Append all the activated instance lists
				tile.AppendVisibleInstanceLists(this, instance_lists_offsets_array);
			}
		}
	}

	void Manager::Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module, job::System* job_system)
	{
		m_device = device;
		m_render_system = render_system;
		m_GPU_memory_render_module = GPU_memory_render_module;
		m_job_system = job_system;

		m_camera_tile_position.i = std::numeric_limits<int32_t>::max();
		m_camera_tile_position.j = std::numeric_limits<int32_t>::max();

		//Build tile descriptors
		GenerateTileDescriptors();

		//Generate zone descriptors
		GenerateZoneDescriptors();

		//Generate building archetypes
		m_building_archetypes.resize(kNumZoneDescriptors);;

		//Create loading thread
		m_loading_thread.reset( new core::Thread(L"TileLoading Thread", core::ThreadPriority::Background, &LoadingThreadRun, this));

		//Simulate one frame in the center, it has to create all tiles around origin
		Update(glm::vec3(0.f, 0.f, 0.f), true);

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

		//Dealloc each building archetype
		for (auto& building_archetype_descriptor : m_building_archetypes)
		{
			for (auto& building_archetype : building_archetype_descriptor)
			{
				m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, building_archetype.handle, render::GetGameFrameIndex(m_render_system));
			}
		}
	}

	void Manager::Update(const glm::vec3& camera_position, bool first_logic_tick_after_render)
	{
		PROFILE_SCOPE("BoxCityTileManager", 0xFFFF77FF, "Update");

		if (!first_logic_tick_after_render)
		{
			//We should not tick more than one each render
			//It can create a lot of data that needs to sync with the GPU, filling a lot of the upload buffers
			return;
		}

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
		return m_tiles[zoneID].GetLodInstancesGPUAllocation(static_cast<LODGroup>(lod_group));
	}

	bool Manager::GetNextTrafficTarget(std::mt19937& random, const glm::vec3& position, glm::vec3& next_target) const
	{
		//Calculate the current tile
		int32_t offset_i = static_cast<int32_t>(floor(2.f * (position.x / kTileSize)));
		int32_t offset_j = static_cast<int32_t>(floor(2.f * (position.y / kTileSize)));
		int32_t offset_k = static_cast<int32_t>(floor(4.f * (position.z - kTileHeightBottom) / (kTileHeightTop - kTileHeightBottom)));
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
		next_target = tile.GetTrafficNextTargetPosition(x, y, z, random());

		assert(glm::distance(glm::vec2(next_target), glm::vec2(position)) < 2000.f);
		return true;
	}

	const Manager::BuildingArchetype& Manager::GetBuildingArchetype(const uint32_t zone_descriptor_index, const uint32_t random_id)
	{
		//Check if it has been already created
		Manager::BuildingArchetype& building_archetype = m_building_archetypes[zone_descriptor_index][random_id % kBuildingsArchetypesPerDescriptor];
		if (!building_archetype.handle.IsValid())
		{
			//It needs to be created
			auto colour_sRGB_to_linear = [](uint8_t r, uint8_t g, uint8_t b)
				{
					float red = r / 255.f;
					float green = g / 255.f;
					float blue = b / 255.f;
					return glm::vec4(glm::pow(red, 2.2f), glm::pow(green, 2.2f), glm::pow(blue, 2.2f), 0.f);
				};

			float emissive_factor = 15.f;
			//Color palette
			const glm::vec4 colour_palette[] =
			{
				emissive_factor * colour_sRGB_to_linear(0x24, 0xFD, 0x36), //Green
				emissive_factor * colour_sRGB_to_linear(0xFF, 0xEF, 0x06), //Yellow
				emissive_factor * colour_sRGB_to_linear(0xFF, 0x3A, 0x06), //Orange
				emissive_factor * colour_sRGB_to_linear(0x0C, 0xD4, 0xFF), //Blue
				emissive_factor * colour_sRGB_to_linear(0xF5, 0x00, 0xEB) //Pink
			};

			std::mt19937 random(zone_descriptor_index * kNumZoneDescriptors + (random_id % kBuildingsArchetypesPerDescriptor));
			const auto& zone_descriptor = kZoneDescriptors[zone_descriptor_index];

			std::uniform_real_distribution<float> length_range(zone_descriptor.length_range_min, zone_descriptor.length_range_max);
			std::uniform_real_distribution<float> size_range(zone_descriptor.size_range_min, zone_descriptor.size_range_max);

			const float size = size_range(random);
			building_archetype.extents = glm::vec3(size, size, length_range(random));

			const float panel_depth = zone_descriptor.panel_depth_panel;

			std::vector<GPUBox> boxes;
			//Build the boxes associated to it
			boxes.emplace_back(glm::vec3(0.f, 0.f, 0.f), building_archetype.extents, glm::vec3(0.05f, 0.05f, 0.05f), 0);

			uint8_t border_colour_palette = random() % 5;

			//Create the boxes that makes this building
			std::vector<std::pair<glm::vec2, glm::vec2>> panels_generated;
			for (size_t face = 0; face < 4; ++face)
			{
				//For each face try to create panels
				const float wall_width = (face % 2 == 0) ? building_archetype.extents.x : building_archetype.extents.y;
				const float wall_heigh = building_archetype.extents.z;
				panels_generated.clear();

				std::uniform_real_distribution<float> panel_size_range(zone_descriptor.panel_size_range_min, glm::min(wall_width, zone_descriptor.panel_size_range_max));

				//Calculate rotation matrix of the face and position
				glm::mat3x3 face_rotation = glm::mat3x3(glm::rotate(glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f))) * glm::mat3x3(glm::rotate(glm::half_pi<float>() * face, glm::vec3(0.f, 0.f, 1.f)));
				glm::vec3 face_position = glm::vec3(0.f, 0.f, wall_width) * face_rotation;

				//Create the borders
				boxes.emplace_back(face_position + glm::vec3(wall_width, 0.f, 0.f) * face_rotation, glm::abs(glm::vec3(panel_depth, wall_heigh, panel_depth) * face_rotation), colour_palette[border_colour_palette], GPUBox::kFlags_Emissive);
				boxes.emplace_back(face_position + glm::vec3(0.f, wall_heigh, 0.f) * face_rotation, glm::abs(glm::vec3(wall_width, panel_depth, panel_depth) * face_rotation), colour_palette[border_colour_palette], GPUBox::kFlags_Emissive);
				boxes.emplace_back(face_position + glm::vec3(0.f, -wall_heigh, 0.f) * face_rotation, glm::abs(glm::vec3(wall_width, panel_depth, panel_depth) * face_rotation), colour_palette[border_colour_palette], GPUBox::kFlags_Emissive);

				for (size_t i = 0; i < zone_descriptor.num_panel_generated; ++i)
				{
					glm::vec2 panel_size(panel_size_range(random), panel_size_range(random));
					std::uniform_real_distribution<float> panel_position_x_range(-wall_width * 0.97f + panel_size.x, wall_width * 0.97f - panel_size.x);
					std::uniform_real_distribution<float> panel_position_y_range(-wall_heigh * 0.97f + panel_size.y, wall_heigh * 0.97f - panel_size.y);
					glm::vec2 panel_position(panel_position_x_range(random), panel_position_y_range(random));

					//Check if it collides
					bool collide = false;
					for (auto& generated_panel : panels_generated)
					{
						//If collides
						if (CollisionPanelVsPanel(panel_position, panel_size, generated_panel.first, generated_panel.second))
						{
							//Next
							collide = true;
							break;
						}
					}

					if (collide)
					{
						continue;
					}

					panels_generated.emplace_back(panel_position, panel_size);

					uint8_t colour_palette_index = random() % 5;
					boxes.emplace_back(face_position + glm::vec3(panel_position.x, panel_position.y, panel_depth / 2.f) * face_rotation, glm::abs(glm::vec3(panel_size.x, panel_size.y, panel_depth / 2.f) * face_rotation), colour_palette[colour_palette_index], GPUBox::kFlags_Emissive);
				}
			}

			//Allocate the gpu memory and initialize it with the building

			//Header
			uint32_t header[4];
			header[0] = static_cast<uint32_t>(boxes.size());
			header[1] = 0;
			header[2] = 0;
			header[3] = 0;


			building_archetype.handle = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, 16 + boxes.size() * sizeof(GPUBox), nullptr, render::GetGameFrameIndex(m_render_system));

			//Upload the header
			m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, building_archetype.handle, header, 16, render::GetGameFrameIndex(m_render_system), 0);
			//Upload the boxes
			m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, building_archetype.handle, boxes.data(), boxes.size() * sizeof(GPUBox), render::GetGameFrameIndex(m_render_system), 16);
		}

		return building_archetype;
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

		//Register extra worker
		job::RegisterExtraWorker(manager->m_job_system, 0);

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
