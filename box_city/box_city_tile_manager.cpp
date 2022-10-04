#include "box_city_tile_manager.h"
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <render/render.h>
#include <render_module/render_module_gpu_memory.h>
#include <core/counters.h>
#include <core/profile.h>

COUNTER(c_Blocks_Count, "Box City", "Number of Blocks", false);
COUNTER(c_Panels_Count, "Box City", "Number of Panels", false);

namespace
{
	bool CollisionBoxVsTile(const helpers::AABB& aabb_box, const helpers::OBB& obb_box, const BoxCityTileManager::Tile& tile)
	{
		//First, check the bbox of the level
		if (helpers::CollisionAABBVsAABB(aabb_box, tile.bounding_box))
		{
			for (auto& current : tile.generated_boxes)
			{
				//Collide
				if (helpers::CollisionAABBVsAABB(current.aabb, aabb_box) && helpers::CollisionOBBVsOBB(current.obb, obb_box))
				{
					return true;
				}
			}
		}
		return false;
	}

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

void BoxCityTileManager::Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module)
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

void BoxCityTileManager::Shutdown()
{
	//Unload each tile
	for (auto& tile : m_tiles)
	{
		if (tile.visible)
		{
			DespawnTile(tile);
		}
	}
}

void BoxCityTileManager::Update(const glm::vec3& camera_position)
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
			bool visible = tile_descriptor.loaded;

			//If we are under the horizon, we don't need LOD2 or LOD1
			if (camera_position.z < kTileHeightTopViewRange && lod > 0)
				visible = false;
			

			//Check if the tile has the different world index
			if ((tile.tile_position.i != world_tile.i || tile.tile_position.j != world_tile.j) && tile.visible || !visible && tile.visible)
			{
				DespawnTile(tile);
				num_tile_changed++;

				core::LogInfo("Tile Local<%i,%i>, World<%i,%i>, unvisible", local_tile.i, local_tile.j, world_tile.i, world_tile.j);
			}

			//If tile is unloaded but it needs to be loaded
			if (!tile.visible && visible && num_tile_changed < max_tile_changed_per_frame)
			{
				//Create new time
				BuildTileData(local_tile, world_tile);
				SpawnTile(tile, lod);
				num_tile_changed++;

				core::LogInfo("Tile Local<%i,%i>, World<%i,%i>, Lod<%i> visible", local_tile.i, local_tile.j, world_tile.i, world_tile.j, lod);
			}

			if (tile.visible && tile.lod != lod && num_tile_changed < max_tile_changed_per_frame)
			{
				//Change lod
				LodTile(tile, lod);
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

render::AllocHandle& BoxCityTileManager::GetGPUHandle(uint32_t zoneID, uint32_t lod_group)
{
	return m_tiles[zoneID].GetLodGPUAllocation(static_cast<LODGroup>(lod_group));
}

void BoxCityTileManager::GenerateTileDescriptors()
{
	//We create all the tile descriptors
	constexpr int32_t range = static_cast<int32_t>(BoxCityTileManager::kLocalTileCount / 2);
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
				tile_descriptor.loaded = true;
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
				//Non loaded
				tile_descriptor.loaded = false;
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

BoxCityTileManager::Tile& BoxCityTileManager::GetTile(const LocalTilePosition& local_tile)
{
	return m_tiles[local_tile.i + local_tile.j * kLocalTileCount];
}

void BoxCityTileManager::BuildTileData(const LocalTilePosition& local_tile, const WorldTilePosition& world_tile)
{
	std::mt19937 random(static_cast<uint32_t>((100000 + world_tile.i) + (100000 + world_tile.j) * BoxCityTileManager::kLocalTileCount));

	std::uniform_real_distribution<float> position_range(0, kTileSize);
	std::uniform_real_distribution<float> position_range_z(kTileHeightBottom, kTileHeightTop);
	std::uniform_real_distribution<float> angle_inc_range(-glm::half_pi<float>() * 0.2f, glm::half_pi<float>() * 0.2f);
	std::uniform_real_distribution<float> angle_rotation_range(0.f, glm::two_pi<float>());
	std::uniform_real_distribution<float> length_range(50.f, 150.f);
	std::uniform_real_distribution<float> size_range(20.0f, 30.0f);

	std::uniform_real_distribution<float> range_animation_range(0.f, 50.f);
	std::uniform_real_distribution<float> frecuency_animation_range(0.3f, 1.f);
	std::uniform_real_distribution<float> offset_animation_range(0.f, 40.f);

	float static_range_box_city = 10.f;

	//Tile positions
	const float begin_tile_x = world_tile.i * kTileSize;
	const float begin_tile_y = world_tile.j * kTileSize;

	BoxCityTileManager::Tile& tile = GetTile(local_tile);

	tile.bounding_box.min = glm::vec3(begin_tile_x, begin_tile_y, kTileHeightTop);
	tile.bounding_box.max = glm::vec3(begin_tile_x + kTileSize, begin_tile_y + kTileSize, kTileHeightBottom);

	tile.zone_id = static_cast<uint16_t>(local_tile.i + local_tile.j * BoxCityTileManager::kLocalTileCount);
	tile.tile_position = world_tile;
	tile.generated_boxes.clear();
	tile.level_data[static_cast<size_t>(LODGroup::TopBuildings)].clear();
	tile.level_data[static_cast<size_t>(LODGroup::TopPanels)].clear();
	tile.level_data[static_cast<size_t>(LODGroup::Rest)].clear();

	//Create boxes
	for (size_t i = 0; i < 350; ++i)
	{
		helpers::OBB obb_box;
		float size = size_range(random);
		obb_box.position = glm::vec3(begin_tile_x + position_range(random), begin_tile_y + position_range(random), position_range_z(random));
		obb_box.extents = glm::vec3(size, size, length_range(random));
		obb_box.rotation = glm::rotate(angle_inc_range(random), glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(angle_rotation_range(random), glm::vec3(0.f, 0.f, 1.f));

		helpers::AABB aabb_box;
		helpers::CalculateAABBFromOBB(aabb_box, obb_box);

		AnimationBox animated_box;
		animated_box.frecuency = frecuency_animation_range(random);
		animated_box.offset = offset_animation_range(random);
		animated_box.range = range_animation_range(random);
		animated_box.original_position = obb_box.position;

		bool dynamic_box = true;
		if (animated_box.range < static_range_box_city)
		{
			dynamic_box = false;
		}

		//Check if it is colliding with another one
		helpers::OBB extended_obb_box = obb_box;
		if (dynamic_box)
		{
			extended_obb_box.extents.z += animated_box.range;
		}
		helpers::AABB extended_aabb_box;
		helpers::CalculateAABBFromOBB(extended_aabb_box, extended_obb_box);

		//First collision in the tile
		bool collide = CollisionBoxVsTile(extended_aabb_box, extended_obb_box, tile);

		
		//Neigbour calculation, is not perfect as it depends of the loading pattern...
		
		//Then neighbours
		for (int32_t ii = world_tile.i - 1; (ii <= (world_tile.i + 1)) && !collide; ++ii)
		{
			for (int32_t jj = world_tile.j - 1; (jj <= (world_tile.j + 1)) && !collide; ++jj)
			{
				if (ii != world_tile.i || jj != world_tile.j)
				{
					//Calculate local tile
					LocalTilePosition local_tile = CalculateLocalTileIndex(WorldTilePosition{ ii, jj });
					const Tile& neighbour_tile = GetTile(local_tile.i, local_tile.j);
					if (neighbour_tile.loaded)
					{
						collide = CollisionBoxVsTile(extended_aabb_box, extended_obb_box, neighbour_tile);
					}
				}
			}
		}
		if (collide)
		{
			//Check another one
			continue;
		}
		else
		{
			//Add this one in the current list
			tile.generated_boxes.push_back({ extended_aabb_box, extended_obb_box });

			//Expand the bbox of the time if needed
			tile.bounding_box.max.x = std::max(tile.bounding_box.max.x, extended_aabb_box.max.x);
			tile.bounding_box.max.y = std::max(tile.bounding_box.max.y, extended_aabb_box.max.y);
			tile.bounding_box.min.x = std::min(tile.bounding_box.min.x, extended_aabb_box.min.x);
			tile.bounding_box.min.y = std::min(tile.bounding_box.min.y, extended_aabb_box.min.y);
		}

		//Block can be build
		BuildBlockData(random, tile, tile.zone_id, obb_box, aabb_box, dynamic_box, animated_box);

		//Gow zone AABB by the bounding box
		tile.bounding_box.min = glm::min(tile.bounding_box.min, extended_aabb_box.min);
		tile.bounding_box.max = glm::max(tile.bounding_box.max, extended_aabb_box.max);
	}

	tile.loaded = true;
	tile.lod = -1;
}

void BoxCityTileManager::BuildBlockData(std::mt19937& random, Tile& tile, const uint16_t zone_id, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box)
{
	//Just a little smaller, so it has space for the panels
	const float panel_depth = 5.0f;

	helpers::OBB updated_obb = obb;
	updated_obb.extents = updated_obb.extents - glm::vec3(panel_depth, panel_depth, 0.f);

	uint32_t parent_index;
	bool top;
	if (!dynamic_box)
	{
		BoxData box_data;
		box_data.aabb_box = aabb;
		box_data.oob_box = updated_obb;

		if (box_data.oob_box.position.z + box_data.oob_box.extents.z > kTileHeightTopViewRange)
		{
			top = true;
			//Top box
			tile.GetLodGroupData(LODGroup::TopBuildings).building_data.push_back(box_data);
		}
		else
		{
			top = false;
			//Bottom box
			tile.GetLodGroupData(LODGroup::Rest).building_data.push_back(box_data);
		}
	}
	else
	{
		AnimatedBoxData animated_box_data;
		animated_box_data.aabb_box = aabb;
		animated_box_data.oob_box = updated_obb;
		animated_box_data.animation = animated_box;
		
		if (animated_box_data.oob_box.position.z + animated_box_data.oob_box.extents.z + animated_box.range > kTileHeightTopViewRange)
		{
			top = true;
			//Top box
			tile.GetLodGroupData(LODGroup::TopBuildings).animated_building_data.push_back(animated_box_data);

			parent_index = static_cast<uint32_t>(tile.GetLodGroupData(LODGroup::TopBuildings).animated_building_data.size() - 1);
		}
		else
		{
			top = false;
			//Bottom box
			tile.GetLodGroupData(LODGroup::Rest).animated_building_data.push_back(animated_box_data);

			parent_index = static_cast<uint32_t>(tile.GetLodGroupData(LODGroup::Rest).animated_building_data.size() - 1);
		}
	}
	//Create panels in each side of the box

	//Matrix used for the attachments
	glm::mat4x4 box_to_world(updated_obb.rotation);
	box_to_world[3] = glm::vec4(updated_obb.position, 1.f);

	std::vector<std::pair<glm::vec2, glm::vec2>> panels_generated;
	for (size_t face = 0; face < 4; ++face)
	{
		//For each face try to create panels
		const float wall_width = (face % 2 == 0) ? updated_obb.extents.x : updated_obb.extents.y;
		const float wall_heigh = updated_obb.extents.z;
		panels_generated.clear();

		std::uniform_real_distribution<float> panel_size_range(5.f, glm::min(wall_width, 15.f));

		//Calculate rotation matrix of the face and position
		glm::mat3x3 face_rotation = glm::mat3x3(glm::rotate(glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f))) * glm::mat3x3(glm::rotate(glm::half_pi<float>() * face, glm::vec3(0.f, 0.f, 1.f))) * updated_obb.rotation;
		glm::vec3 face_position = updated_obb.position + glm::vec3(0.f, 0.f, wall_width) * face_rotation;

		for (size_t i = 0; i < 16; ++i)
		{
			glm::vec2 panel_size(panel_size_range(random), panel_size_range(random));
			std::uniform_real_distribution<float> panel_position_x_range(-wall_width + panel_size.x, wall_width - panel_size.x);
			std::uniform_real_distribution<float> panel_position_y_range(-wall_heigh + panel_size.y, wall_heigh - panel_size.y);
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

			//Calculate panel data
			helpers::AABB aabb_panel;
			helpers::OBB obb_panel;
			obb_panel.position = face_position + glm::vec3(panel_position.x, panel_position.y, panel_depth / 2.f) * face_rotation;
			obb_panel.rotation = face_rotation;
			obb_panel.extents = glm::vec3(panel_size.x, panel_size.y, panel_depth / 2.f);

			helpers::CalculateAABBFromOBB(aabb_panel, obb_panel);

			uint8_t colour_palette = random() % 5;

			if (dynamic_box)
			{
				//Calculate attachment matrix
				AnimatedPanelData animated_panel_data;

				animated_panel_data.parent_index = parent_index;
				animated_panel_data.aabb_box = aabb_panel;
				animated_panel_data.oob_box = obb_panel;
				animated_panel_data.colour_palette = colour_palette;

				glm::mat4x4 panel_to_world(obb_panel.rotation);
				panel_to_world[3] = glm::vec4(obb_panel.position, 1.f);

				animated_panel_data.parent_to_child = glm::inverse(box_to_world) * panel_to_world;

				if (top)
				{
					tile.GetLodGroupData(LODGroup::TopPanels).animated_panel_data.push_back(animated_panel_data);
				}
				else
				{
					tile.GetLodGroupData(LODGroup::Rest).animated_panel_data.push_back(animated_panel_data);
				}
			}
			else
			{
				PanelData panel_data;

				panel_data.aabb_box = aabb_panel;
				panel_data.oob_box = obb_panel;
				panel_data.colour_palette = colour_palette;

				if (top)
				{
					tile.GetLodGroupData(LODGroup::TopPanels).panel_data.push_back(panel_data);
				}
				else
				{
					tile.GetLodGroupData(LODGroup::Rest).panel_data.push_back(panel_data);
				}
			}
		}
	}
}

void BoxCityTileManager::SpawnLodGroup(Tile& tile, const LODGroup lod_group)
{
	LODGroupData& lod_group_data = tile.GetLodGroupData(lod_group);
	auto& instances_vector = tile.GetLodInstances(lod_group);

	//Size of the GPU allocation
	size_t gpu_allocation_size = sizeof(GPUBoxInstance) * (lod_group_data.animated_building_data.size() + lod_group_data.building_data.size() + lod_group_data.animated_panel_data.size() + lod_group_data.panel_data.size());
	
	//Allocate the GPU memory
	tile.GetLodGPUAllocation(lod_group) = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, gpu_allocation_size, nullptr, render::GetGameFrameIndex(m_render_system));

	//Each instance it will reserve like a lineal allocator
	uint32_t gpu_offset = 0;

	//First dynamic
	for (auto& building_data : lod_group_data.animated_building_data)
	{
		BoxRender box_render;
		box_render.colour = glm::vec4(1.f, 1.f, 1.f, 0.f);

		//GPU memory
		GPUBoxInstance gpu_box_instance;
		gpu_box_instance.Fill(building_data.oob_box);
		gpu_box_instance.Fill(box_render);

		//Update the GPU memory
		m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, tile.GetLodGPUAllocation(lod_group), &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system), gpu_offset * sizeof(GPUBoxInstance));

		//Just make it static
		instances_vector.push_back(ecs::AllocInstance<GameDatabase, AnimatedBoxType>(tile.zone_id)
			.Init<OBBBox>(building_data.oob_box)
			.Init<AABBBox>(building_data.aabb_box)
			.Init<BoxRender>(box_render)
			.Init<AnimationBox>(building_data.animation)
			.Init<BoxGPUHandle>(gpu_offset, static_cast<uint32_t>(lod_group)));

		gpu_offset++;
		COUNTER_INC(c_Blocks_Count);
	}

	for (auto& building_data : lod_group_data.building_data)
	{
		BoxRender box_render;
		box_render.colour = glm::vec4(1.f, 1.f, 1.f, 0.f);

		//GPU memory
		GPUBoxInstance gpu_box_instance;
		gpu_box_instance.Fill(building_data.oob_box);
		gpu_box_instance.Fill(box_render);

		//Update the GPU memory
		m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, tile.GetLodGPUAllocation(lod_group), &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system), gpu_offset * sizeof(GPUBoxInstance));

		//Just make it static
		instances_vector.push_back(ecs::AllocInstance<GameDatabase, BoxType>(tile.zone_id)
			.Init<OBBBox>(building_data.oob_box)
			.Init<AABBBox>(building_data.aabb_box)
			.Init<BoxRender>(box_render)
			.Init<BoxGPUHandle>(gpu_offset, static_cast<uint32_t>(lod_group)));

		gpu_offset++;
		COUNTER_INC(c_Blocks_Count);
	}

	//Color palette
	const glm::vec4 colour_palette[] =
	{
		{1.f, 0.1f, 0.6f, 0.f},
		{1.f, 0.6f, 0.1f, 0.f},
		{1.f, 0.95f, 0.f, 0.f},
		{0.5f, 1.f, 0.f, 0.f},
		{0.f, 1.0f, 1.f, 0.f}
	};

	for (auto& panel_data : lod_group_data.animated_panel_data)
	{
		glm::mat4x4 box_to_world;
		//Calculate attachment matrix
		Attachment attachment;
		if (lod_group == LODGroup::TopPanels)
		{
			//Parents are in the TopBuilding groups
			attachment.parent = tile.GetLodInstances(LODGroup::TopBuildings)[panel_data.parent_index];
			box_to_world = tile.GetLodGroupData(LODGroup::TopBuildings).animated_building_data[panel_data.parent_index].oob_box.rotation;
			box_to_world[3] = glm::vec4(tile.GetLodGroupData(LODGroup::TopBuildings).animated_building_data[panel_data.parent_index].oob_box.position, 1.f);
		}
		else
		{
			//Parents are in this group
			attachment.parent = instances_vector[panel_data.parent_index];
			box_to_world = lod_group_data.animated_building_data[panel_data.parent_index].oob_box.rotation;
			box_to_world[3] = glm::vec4(lod_group_data.animated_building_data[panel_data.parent_index].oob_box.position, 1.f);
		}

		glm::mat4x4 panel_to_world(panel_data.oob_box.rotation);
		panel_to_world[3] = glm::vec4(panel_data.oob_box.position, 1.f);

		attachment.parent_to_child = glm::inverse(box_to_world) * panel_to_world;

		BoxRender box_render;
		box_render.colour = colour_palette[panel_data.colour_palette] * 2.f;

		//GPU memory
		GPUBoxInstance gpu_box_instance;
		gpu_box_instance.Fill(panel_data.oob_box);
		gpu_box_instance.Fill(box_render);

		//Update the GPU memory
		m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, tile.GetLodGPUAllocation(lod_group), &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system), gpu_offset * sizeof(GPUBoxInstance));

		//Add attached
		instances_vector.push_back(ecs::AllocInstance<GameDatabase, AttachedPanelType>(tile.zone_id)
			.Init<OBBBox>(panel_data.oob_box)
			.Init<AABBBox>(panel_data.aabb_box)
			.Init<BoxRender>(box_render)
			.Init<Attachment>(attachment)
			.Init<BoxGPUHandle>(gpu_offset, static_cast<uint32_t>(lod_group)));

		gpu_offset++;
		COUNTER_INC(c_Panels_Count);
	}

	for (auto& panel_data : lod_group_data.panel_data)
	{
		BoxRender box_render;
		box_render.colour = colour_palette[panel_data.colour_palette];

		//GPU memory
		GPUBoxInstance gpu_box_instance;
		gpu_box_instance.Fill(panel_data.oob_box);
		gpu_box_instance.Fill(box_render);

		//Update the GPU memory
		m_GPU_memory_render_module->UpdateStaticGPUMemory(m_device, tile.GetLodGPUAllocation(lod_group), &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(m_render_system), gpu_offset * sizeof(GPUBoxInstance));

		//Add attached
		instances_vector.push_back(ecs::AllocInstance<GameDatabase, PanelType>(tile.zone_id)
			.Init<OBBBox>(panel_data.oob_box)
			.Init<AABBBox>(panel_data.aabb_box)
			.Init<BoxRender>(box_render)
			.Init<BoxGPUHandle>(gpu_offset, static_cast<uint32_t>(lod_group)));

		gpu_offset++;
		COUNTER_INC(c_Panels_Count);
	}
}

void BoxCityTileManager::SpawnTile(Tile& tile, uint32_t lod)
{
	//LOD 0 has Rest
	//LOD 1 has Rest, TopBuildings and TopPanels
	//LOD 2 has TopBuildings and Top Panels
	//LOD 3 has TopBuildings only
	// 
	//Depends of the LOD, it spawns the instances the ECS
	assert(tile.loaded);

	LodTile(tile, lod);
	tile.visible = true;
}

void BoxCityTileManager::DespawnLodGroup(Tile& tile, const LODGroup lod_group)
{
	for (auto& instance : tile.GetLodInstances(lod_group))
	{
		//Check the type
		if (instance.Is<BoxType>() || instance.Is<AnimatedBoxType>())
		{
			COUNTER_SUB(c_Blocks_Count);
		}
		else
		{
			COUNTER_SUB(c_Panels_Count);
		}
		//Set to invalid to the GPU memory
		instance.Get<BoxGPUHandle>().offset_gpu_allocator = BoxGPUHandle::kInvalidOffset;

		//Dealloc instance
		ecs::DeallocInstance<GameDatabase>(instance);
	}
	tile.GetLodInstances(lod_group).clear();
	
	//Dealloc the gpu handle
	if (tile.GetLodGPUAllocation(lod_group).IsValid())
	{
		m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, tile.GetLodGPUAllocation(lod_group), render::GetGameFrameIndex(m_render_system));
	}
}


void BoxCityTileManager::DespawnTile(Tile& tile)
{
	//Removes all the instances left in the ECS
	DespawnLodGroup(tile, LODGroup::TopBuildings);
	DespawnLodGroup(tile, LODGroup::TopPanels);
	DespawnLodGroup(tile, LODGroup::Rest);

	tile.visible = false;
}

void BoxCityTileManager::LodTile(Tile& tile, uint32_t new_lod)
{
	if (tile.lod != new_lod)
	{
		//For each group lod, check what it needs to be done
		const uint32_t next_lod_mask = kLodMask[new_lod];
		uint32_t prev_lod_mask;
		if (tile.lod == -1)
		{
			//Nothing was loaded
			prev_lod_mask = 0;
		}
		else
		{
			prev_lod_mask = kLodMask[tile.lod];
		}

		for (uint32_t i = 0; i < static_cast<uint32_t>(LODGroup::Count); ++i)
		{
			const LODGroup lod_group = static_cast<LODGroup>(i);
			if ((prev_lod_mask & (1 << i)) != 0 && (next_lod_mask & (1 << i)) == 0)
			{
				//We need to despawn the lod group
				DespawnLodGroup(tile, lod_group);
			}
			if ((prev_lod_mask & (1 << i)) == 0 && (next_lod_mask & (1 << i)) != 0)
			{
				//We need to spawn the lod group
				SpawnLodGroup(tile, lod_group);
			}
		}

		tile.lod = new_lod;
	}
}
