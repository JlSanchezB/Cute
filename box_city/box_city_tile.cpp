#include "box_city_tile.h"
#include "box_city_tile_manager.h"
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <render/render.h>
#include <render_module/render_module_gpu_memory.h>
#include <core/counters.h>
#include "box_city_descriptors.h"

COUNTER(c_Blocks_Count, "Box City", "Number of Blocks", false);
COUNTER(c_Panels_Count, "Box City", "Number of Panels", false);

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
	bool Tile::CollisionBoxVsLoadedTile(const helpers::AABB& aabb_box, const helpers::OBB& obb_box) const
	{
		//First, check the bbox of the level
		if (helpers::CollisionAABBVsAABB(aabb_box, m_bounding_box))
		{
			bool collide = false;
			m_generated_boxes_bvh.Visit(aabb_box, [&](const BoxCollision& box_collision)
				{
					//Collide, aabb has already tested
					if (helpers::CollisionOBBVsOBB(box_collision.obb, obb_box))
					{
						collide = true;
					}
				});

			return collide;
		}
		return false;
	}

	bool Tile::CollisionBoxVsLoadingTile(const helpers::AABB& aabb_box, const helpers::OBB& obb_box) const
	{
		//First, check the bbox of the level
		if (helpers::CollisionAABBVsAABB(aabb_box, m_bounding_box))
		{
			for (auto& current : m_generated_boxes)
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

	void Tile::BuildTileData(Manager* manager, const LocalTilePosition& local_tile, const WorldTilePosition& world_tile)
	{
		std::mt19937 random(static_cast<uint32_t>((100000 + world_tile.i) + (100000 + world_tile.j) * kLocalTileCount));

		std::uniform_real_distribution<float> position_range(0, kTileSize);
		std::uniform_real_distribution<float> position_range_z(kTileHeightBottom, kTileHeightTop);
		
		//Tile positions
		const float begin_tile_x = world_tile.i * kTileSize;
		const float begin_tile_y = world_tile.j * kTileSize;

		m_bounding_box.min = glm::vec3(begin_tile_x, begin_tile_y, kTileHeightTop);
		m_bounding_box.max = glm::vec3(begin_tile_x + kTileSize, begin_tile_y + kTileSize, kTileHeightBottom);

		m_zone_id = static_cast<uint16_t>(local_tile.i + local_tile.j * kLocalTileCount);
		m_tile_position = world_tile;
		m_generated_boxes.clear();
		m_level_data[static_cast<size_t>(LODGroup::TopBuildings)].clear();
		m_level_data[static_cast<size_t>(LODGroup::TopPanels)].clear();
		m_level_data[static_cast<size_t>(LODGroup::Rest)].clear();

		//Create boxes
		for (size_t i = 0; i < 350; ++i)
		{
			helpers::OBB obb_box;
			
			obb_box.position = glm::vec3(begin_tile_x + position_range(random), begin_tile_y + position_range(random), position_range_z(random));
			std::optional<uint32_t> descriptor_index = manager->GetZoneDescriptorIndex(obb_box.position);

			if (!descriptor_index.has_value())
			{
				//It is a corridor
				continue;
			}

			const ZoneDescriptor& zone_descriptor = kZoneDescriptors[descriptor_index.value()];

			std::uniform_real_distribution<float> angle_inc_range(zone_descriptor.angle_inc_range_min, zone_descriptor.angle_inc_range_max);
			std::uniform_real_distribution<float> angle_rotation_range(0.f, glm::two_pi<float>());
			std::uniform_real_distribution<float> length_range(zone_descriptor.length_range_min, zone_descriptor.length_range_max);
			std::uniform_real_distribution<float> size_range(zone_descriptor.size_range_min, zone_descriptor.size_range_max);

			std::uniform_real_distribution<float> range_animation_range(zone_descriptor.animation_distance_range_min, zone_descriptor.animation_distance_range_max);
			std::uniform_real_distribution<float> frecuency_animation_range(zone_descriptor.animation_frecuency_range_min, zone_descriptor.animation_frecuency_range_max);
			std::uniform_real_distribution<float> offset_animation_range(zone_descriptor.animation_offset_range_min, zone_descriptor.animation_offset_range_max);


			float size = size_range(random);
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
			if (animated_box.range < zone_descriptor.static_range)
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
			bool collide = CollisionBoxVsLoadingTile(extended_aabb_box, extended_obb_box);


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
						const Tile& neighbour_tile = manager->GetTile(local_tile.i, local_tile.j);
						if (neighbour_tile.IsLoaded())
						{
							collide = neighbour_tile.CollisionBoxVsLoadedTile(extended_aabb_box, extended_obb_box);
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
				m_generated_boxes.push_back({ extended_aabb_box, extended_obb_box });
			}

			//Block can be build
			BuildBlockData(random, obb_box, aabb_box, dynamic_box, animated_box, descriptor_index.value());

			//Gow zone AABB by the bounding box
			m_bounding_box.Add(extended_aabb_box);
		}

		//Build the BVH
		m_generated_boxes_bvh.Build(m_generated_boxes.data(), static_cast<uint32_t>(m_generated_boxes.size()), m_bounding_box);

		assert(m_state == State::Loaded || m_state == State::Unloaded || m_state == State::Loading);
		SetState(State::Loaded);
		m_lod = -1;
	}

	void Tile::BuildBlockData(std::mt19937& random, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box, uint32_t descriptor_index)
	{
		const ZoneDescriptor& zone_descriptor = kZoneDescriptors[descriptor_index];

		//Just a little smaller, so it has space for the panels
		const float panel_depth = zone_descriptor.panel_depth_panel;

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
				GetLodGroupData(LODGroup::TopBuildings).building_data.push_back(box_data);
			}
			else
			{
				top = false;
				//Bottom box
				GetLodGroupData(LODGroup::Rest).building_data.push_back(box_data);
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
				GetLodGroupData(LODGroup::TopBuildings).animated_building_data.push_back(animated_box_data);

				parent_index = static_cast<uint32_t>(GetLodGroupData(LODGroup::TopBuildings).animated_building_data.size() - 1);
			}
			else
			{
				top = false;
				//Bottom box
				GetLodGroupData(LODGroup::Rest).animated_building_data.push_back(animated_box_data);

				parent_index = static_cast<uint32_t>(GetLodGroupData(LODGroup::Rest).animated_building_data.size() - 1);
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

			std::uniform_real_distribution<float> panel_size_range(zone_descriptor.panel_size_range_min, glm::min(wall_width, zone_descriptor.panel_size_range_max));

			//Calculate rotation matrix of the face and position
			glm::mat3x3 face_rotation = glm::mat3x3(glm::rotate(glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f))) * glm::mat3x3(glm::rotate(glm::half_pi<float>() * face, glm::vec3(0.f, 0.f, 1.f))) * updated_obb.rotation;
			glm::vec3 face_position = updated_obb.position + glm::vec3(0.f, 0.f, wall_width) * face_rotation;

			for (size_t i = 0; i < zone_descriptor.num_panel_generated; ++i)
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
						GetLodGroupData(LODGroup::TopPanels).animated_panel_data.push_back(animated_panel_data);
					}
					else
					{
						GetLodGroupData(LODGroup::Rest).animated_panel_data.push_back(animated_panel_data);
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
						GetLodGroupData(LODGroup::TopPanels).panel_data.push_back(panel_data);
					}
					else
					{
						GetLodGroupData(LODGroup::Rest).panel_data.push_back(panel_data);
					}
				}
			}
		}
	}

	void Tile::SpawnLodGroup(Manager* manager, const LODGroup lod_group)
	{
		LODGroupData& lod_group_data = GetLodGroupData(lod_group);
		auto& instances_vector = GetLodInstances(lod_group);

		//Size of the GPU allocation
		size_t gpu_allocation_size = sizeof(GPUBoxInstance) * (lod_group_data.animated_building_data.size() + lod_group_data.building_data.size() + lod_group_data.animated_panel_data.size() + lod_group_data.panel_data.size());

		if (gpu_allocation_size == 0)
		{
			return;
		}

		//Allocate the GPU memory
		GetLodGPUAllocation(lod_group) = manager->GetGPUMemoryRenderModule()->AllocStaticGPUMemory(manager->GetDevice(), gpu_allocation_size, nullptr, render::GetGameFrameIndex(manager->GetRenderSystem()));

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
			manager->GetGPUMemoryRenderModule()->UpdateStaticGPUMemory(manager->GetDevice(), GetLodGPUAllocation(lod_group), &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(manager->GetRenderSystem()), gpu_offset * sizeof(GPUBoxInstance));

			//Just make it static
			instances_vector.push_back(ecs::AllocInstance<GameDatabase, AnimatedBoxType>(m_zone_id)
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
			manager->GetGPUMemoryRenderModule()->UpdateStaticGPUMemory(manager->GetDevice(), GetLodGPUAllocation(lod_group), &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(manager->GetRenderSystem()), gpu_offset * sizeof(GPUBoxInstance));

			//Just make it static
			instances_vector.push_back(ecs::AllocInstance<GameDatabase, BoxType>(m_zone_id)
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
				attachment.parent = GetLodInstances(LODGroup::TopBuildings)[panel_data.parent_index];
				box_to_world = GetLodGroupData(LODGroup::TopBuildings).animated_building_data[panel_data.parent_index].oob_box.rotation;
				box_to_world[3] = glm::vec4(GetLodGroupData(LODGroup::TopBuildings).animated_building_data[panel_data.parent_index].oob_box.position, 1.f);
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
			manager->GetGPUMemoryRenderModule()->UpdateStaticGPUMemory(manager->GetDevice(), GetLodGPUAllocation(lod_group), &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(manager->GetRenderSystem()), gpu_offset * sizeof(GPUBoxInstance));

			//Add attached
			instances_vector.push_back(ecs::AllocInstance<GameDatabase, AttachedPanelType>(m_zone_id)
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
			manager->GetGPUMemoryRenderModule()->UpdateStaticGPUMemory(manager->GetDevice(), GetLodGPUAllocation(lod_group), &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(manager->GetRenderSystem()), gpu_offset * sizeof(GPUBoxInstance));

			//Add attached
			instances_vector.push_back(ecs::AllocInstance<GameDatabase, PanelType>(m_zone_id)
				.Init<OBBBox>(panel_data.oob_box)
				.Init<AABBBox>(panel_data.aabb_box)
				.Init<BoxRender>(box_render)
				.Init<BoxGPUHandle>(gpu_offset, static_cast<uint32_t>(lod_group)));

			gpu_offset++;
			COUNTER_INC(c_Panels_Count);
		}
	}

	void Tile::SpawnTile(Manager* manager, uint32_t lod)
	{
		//LOD 0 has Rest
		//LOD 1 has Rest, TopBuildings and TopPanels
		//LOD 2 has TopBuildings and Top Panels
		//LOD 3 has TopBuildings only
		// 
		//Depends of the LOD, it spawns the instances the ECS

		LodTile(manager, lod);
		assert(m_state == State::Loaded);
		SetState(State::Visible);
	}

	void Tile::DespawnLodGroup(Manager* manager, const LODGroup lod_group)
	{
		for (auto& instance : GetLodInstances(lod_group))
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
		GetLodInstances(lod_group).clear();

		//Dealloc the gpu handle
		if (GetLodGPUAllocation(lod_group).IsValid())
		{
			manager->GetGPUMemoryRenderModule()->DeallocStaticGPUMemory(manager->GetDevice(), GetLodGPUAllocation(lod_group), render::GetGameFrameIndex(manager->GetRenderSystem()));
		}
	}


	void Tile::DespawnTile(Manager* manager)
	{
		//Removes all the instances left in the ECS
		DespawnLodGroup(manager, LODGroup::TopBuildings);
		DespawnLodGroup(manager, LODGroup::TopPanels);
		DespawnLodGroup(manager, LODGroup::Rest);

		m_lod = -1;

		assert(m_state == State::Visible);
		SetState(State::Loaded);
	}

	void Tile::LodTile(Manager* manager, uint32_t new_lod)
	{
		if (m_lod != new_lod)
		{
			//For each group lod, check what it needs to be done
			const uint32_t next_lod_mask = kLodMask[new_lod];
			uint32_t prev_lod_mask;
			if (m_lod == -1)
			{
				//Nothing was m_loaded
				prev_lod_mask = 0;
			}
			else
			{
				prev_lod_mask = kLodMask[m_lod];
			}

			for (uint32_t i = 0; i < static_cast<uint32_t>(LODGroup::Count); ++i)
			{
				const LODGroup lod_group = static_cast<LODGroup>(i);
				if ((prev_lod_mask & (1 << i)) != 0 && (next_lod_mask & (1 << i)) == 0)
				{
					//We need to despawn the lod group
					DespawnLodGroup(manager, lod_group);
				}
				if ((prev_lod_mask & (1 << i)) == 0 && (next_lod_mask & (1 << i)) != 0)
				{
					//We need to spawn the lod group
					SpawnLodGroup(manager, lod_group);
				}
			}

			m_lod = new_lod;
		}
	}
	void Tile::AddedToLoadingQueue()
	{
		assert(m_state == State::Unloaded || m_state == State::Loaded);
		SetState(State::Loading);
	}
}