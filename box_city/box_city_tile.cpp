#include "box_city_tile.h"
#include "box_city_tile_manager.h"
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <render/render.h>
#include <render_module/render_module_gpu_memory.h>
#include <core/counters.h>
#include "box_city_descriptors.h"
#include <numeric>

COUNTER(c_BuildingInstances_Count, "Box City", "Number of building instances", false);
COUNTER(c_Box_Count, "Box City", "Number of box between all the instances", false);
COUNTER(c_Building_Summitted, "Box City", "Building summitted to the GPU", true);

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
			m_generated_boxes_bvh.Visit(aabb_box, [&](const uint32_t& index)
				{
					//Collide, aabb has already tested
					if (helpers::CollisionOBBVsOBB(m_generated_boxes[index].obb, obb_box))
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

	//Fills the target positions for a tile, the tile doesn't need to be loaded
	inline void FillTargetPositions(const WorldTilePosition& world_tile, std::array<glm::vec3, 16>& target_positions)
	{
		std::mt19937 random(static_cast<uint32_t>((100000 + world_tile.i) + (100000 + world_tile.j) * kLocalTileCount));
		
		//Tile positions
		const float begin_tile_x = world_tile.i * kTileSize;
		const float begin_tile_y = world_tile.j * kTileSize;

		std::uniform_real_distribution<float> position_range(0, kTileSize);
		std::uniform_real_distribution<float> position_range_z(kTileHeightBottom, kTileHeightTop);
		std::uniform_real_distribution<float> target_position_offset(0.1f, 0.9f);
		for (uint32_t j = 0; j < 16; j++)
		{
			uint32_t x = j % 2;
			uint32_t y = (j % 4) / 2;
			uint32_t z = j / 4;

			//Get a random position
			glm::vec3 position = glm::vec3(begin_tile_x + (static_cast<float>(x) * 0.5f + target_position_offset(random) * 0.5f) * kTileSize,
				begin_tile_y + (static_cast<float>(y) * 0.5f + target_position_offset(random) * 0.5f) * kTileSize,
				kTileHeightBottom + (kTileHeightTop - kTileHeightBottom) * (static_cast<float>(z) * 0.25f + target_position_offset(random) * 0.25f));

			target_positions[j] = position;
		}
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

		//Calculate the target positions for traffic
		std::array< std::array<glm::vec3, 16>, 9> target_positions;

		FillTargetPositions(world_tile, target_positions[4]);
		FillTargetPositions(WorldTilePosition{ world_tile.i, world_tile.j + 1 }, target_positions[7]);
		FillTargetPositions(WorldTilePosition{ world_tile.i, world_tile.j - 1 }, target_positions[1]);
		FillTargetPositions(WorldTilePosition{ world_tile.i + 1, world_tile.j}, target_positions[5]);
		FillTargetPositions(WorldTilePosition{ world_tile.i - 1, world_tile.j}, target_positions[3]);

		for (uint32_t j = 0; j < 16; j++)
		{
			m_traffic_targets[j].position = target_positions[4][j];

			for (uint32_t k = 0; k < 6; k++)
			{
				// 2,2 is the middle 
				int32_t world_i = 2 + j % 2;
				int32_t world_j = 2 + (j % 4) / 2;
				int32_t world_k = j / 4;

				switch (k)
				{
				case 0: world_k++; break; //UP
				case 1: world_k--; break; //DOWN
				case 2: world_i--; break; //LEFT
				case 3: world_i++; break; //RIGHT
				case 4: world_j--; break; //FAR
				case 5: world_j++; break; //CLOSE
				}
				if (world_k > 3) world_k = 3;
				if (world_k < 0) world_k = 0;

				//Calculate the next target	
				uint32_t tile_i = world_i / 2;
				uint32_t tile_j = world_j / 2;

				assert(!(tile_i == 0 && tile_j == 0));
				assert(!(tile_i == 2 && tile_j == 0));
				assert(!(tile_i == 0 && tile_j == 2));
				assert(!(tile_i == 2 && tile_j == 2));

				uint32_t offset_i = world_i % 2;
				uint32_t offset_j = world_j % 2;
				uint32_t offset_k = world_k;

				m_traffic_targets[j].next_position[k] = target_positions[tile_i + tile_j * 3][offset_i + offset_j * 2 + offset_k * 4];
			}
		}


		//Create boxes
		for (size_t i = 0; i < 450; ++i)
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

			//Collide with a target position
			bool collide = false;
			constexpr float kTargetCleanRadius = 50.f;
			for (auto& traffic_targets : m_traffic_targets)
			{
				auto& target_position = traffic_targets.position;
				//Calculate distance between the closest position and the target position, needs to be over kTargetRadius
				bool inside;
				glm::vec3 closest_point = helpers::CalculateClosestPointToOBB(target_position, extended_obb_box, inside);
				collide = (glm::distance2(closest_point, target_position) < kTargetCleanRadius * kTargetCleanRadius);
				if (collide) break;
			}
			if (collide) continue;

			//First collision in the tile
			collide = CollisionBoxVsLoadingTile(extended_aabb_box, extended_obb_box);
			if (collide) continue;


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

			//Block can be build, the aabb will be the extended one of it is an animated box
			BuildBlockData(random, obb_box, (dynamic_box) ? extended_aabb_box : aabb_box, dynamic_box, animated_box, descriptor_index.value());

			//Gow zone AABB by the bounding box
			m_bounding_box.Add(extended_aabb_box);
		}

		//Build the BVH
		std::vector<uint32_t> indexes(m_generated_boxes.size());
		std::iota(indexes.begin(), indexes.end(), 0);
		LinearBVHGeneratedBoxesSettings bvh_settings(m_generated_boxes);
		m_generated_boxes_bvh.Build(&bvh_settings, indexes.data(), static_cast<uint32_t>(m_generated_boxes.size()), m_bounding_box);

		assert(m_state == State::Loaded || m_state == State::Unloaded || m_state == State::Loading);
		SetState(State::Loaded);
		m_lod = -1;
	}

	void Tile::BuildBlockData(std::mt19937& random, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box, uint32_t descriptor_index)
	{
		const ZoneDescriptor& zone_descriptor = kZoneDescriptors[descriptor_index];

		//Just a little smaller, so it has space for the panels
		const float panel_depth = zone_descriptor.panel_depth_panel;

		bool top = (obb.position.z + obb.extents.z > kTileHeightTopViewRange);

		//Build the instance data
		InstanceData building_instance;
		building_instance.oob_box = obb;

		//Add bulding box
		BoxData building_box;
		building_box.colour_palette = 0xFF;
		building_box.position = glm::vec3(0.f, 0.f, 0.f);
		building_box.extents = obb.extents;
		building_box.extents -= glm::vec3(panel_depth, panel_depth, 0.f);

		building_instance.m_boxes.push_back(building_box);

		assert(building_box.extents.x > 0.f);
		assert(building_box.extents.y > 0.f);
		assert(building_box.extents.z > 0.f);

		uint8_t border_colour_palette = random() % 5;

		//Create the boxes that makes this building
		std::vector<std::pair<glm::vec2, glm::vec2>> panels_generated;
		for (size_t face = 0; face < 4; ++face)
		{
			//For each face try to create panels
			const float wall_width = (face % 2 == 0) ? building_box.extents.x : building_box.extents.y;
			const float wall_heigh = building_box.extents.z;
			panels_generated.clear();

			std::uniform_real_distribution<float> panel_size_range(zone_descriptor.panel_size_range_min, glm::min(wall_width, zone_descriptor.panel_size_range_max));

			//Calculate rotation matrix of the face and position
			glm::mat3x3 face_rotation = glm::mat3x3(glm::rotate(glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f))) * glm::mat3x3(glm::rotate(glm::half_pi<float>() * face, glm::vec3(0.f, 0.f, 1.f)));
			glm::vec3 face_position = glm::vec3(0.f, 0.f, wall_width) * face_rotation;

			//Create the borders
			BoxData border_box;
			border_box.colour_palette = border_colour_palette;
			border_box.position = face_position + glm::vec3(wall_width, 0.f, 0.f) * face_rotation;
			border_box.extents = glm::abs(glm::vec3(panel_depth, wall_heigh, panel_depth) * face_rotation);
			building_instance.m_boxes.push_back(border_box);

			border_box.position = face_position + glm::vec3(0.f, wall_heigh, 0.f) * face_rotation;
			border_box.extents = glm::abs(glm::vec3(wall_width, panel_depth, panel_depth) * face_rotation);
			building_instance.m_boxes.push_back(border_box);

			border_box.position = face_position + glm::vec3(0.f, -wall_heigh, 0.f) * face_rotation;
			border_box.extents = glm::abs(glm::vec3(wall_width, panel_depth, panel_depth) * face_rotation);
			building_instance.m_boxes.push_back(border_box);

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


				uint8_t colour_palette = random() % 5;
				BoxData panel_box;

				panel_box.colour_palette = colour_palette;
				panel_box.position = face_position + glm::vec3(panel_position.x, panel_position.y, panel_depth / 2.f) * face_rotation;
				panel_box.extents = glm::abs(glm::vec3(panel_size.x, panel_size.y, panel_depth / 2.f) * face_rotation);
				
				building_instance.m_boxes.push_back(panel_box);
			}
		}

		if (!dynamic_box)
		{
			if (top)
			{
				//Top box
				GetLodGroupData(LODGroup::TopBuildings).building_data.push_back(std::move(building_instance));
			}
			else
			{
				//Bottom box
				GetLodGroupData(LODGroup::Rest).building_data.push_back(std::move(building_instance));
			}
		}
		else
		{
			//We create an animated from the instance data
			AnimatedInstanceData animated_building_instance(std::move(building_instance));
			animated_building_instance.animation = animated_box;

			if (top)
			{
				//Top box
				GetLodGroupData(LODGroup::TopBuildings).animated_building_data.push_back(std::move(animated_building_instance));
			}
			else
			{
				//Bottom box
				GetLodGroupData(LODGroup::Rest).animated_building_data.push_back(std::move(animated_building_instance));
			}
		}
	}

	render::AllocHandle Tile::CreateBoxList(Manager* manager, const std::vector<BoxData>& box_data_array)
	{
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

		size_t allocation_size = 16 + sizeof(GPUBox) * box_data_array.size();

		//Create the memory with the box list
		std::vector<uint8_t> buffer(allocation_size);

		//Set the size
		uint32_t* size_data = reinterpret_cast<uint32_t*>(buffer.data());
		size_data[0] = static_cast<uint32_t>(box_data_array.size());
		size_data[1] = 0;
		size_data[2] = 0;
		size_data[3] = 0;

		for (size_t i = 0; i < box_data_array.size(); ++i)
		{
			const auto& box_data = box_data_array[i];

			uint8_t* dest_data = buffer.data() + 16 + i * sizeof(GPUBox);
			reinterpret_cast<GPUBox*>(dest_data)->Fill(box_data.position, box_data.extents, (box_data.colour_palette == 0xFF) ? glm::vec3(0.05f, 0.05f, 0.05f) : colour_palette[box_data.colour_palette], (box_data.colour_palette == 0xFF) ? 0 : GPUBox::kFlags_Emissive);
		}

		//COUNTER_INC(c_Box_Count);

		//Allocate it
		return manager->GetGPUMemoryRenderModule()->AllocStaticGPUMemory(manager->GetDevice(), allocation_size, buffer.data(), render::GetGameFrameIndex(manager->GetRenderSystem()));
	}

	void Tile::SpawnLodGroup(Manager* manager, const LODGroup lod_group)
	{
		LODGroupData& lod_group_data = GetLodGroupData(lod_group);
		auto& instances_vector = GetLodInstances(lod_group);
		auto& instances_gpu_allocation = GetLodInstancesGPUAllocation(lod_group);
		auto& instance_list_gpu_allocation = GetLodInstanceListGPUAllocation(lod_group);

		//Size of the GPU allocation
		size_t num_box_instances = (lod_group_data.animated_building_data.size() + lod_group_data.building_data.size());
		size_t gpu_allocation_size = sizeof(GPUBoxInstance) * num_box_instances;

		if (gpu_allocation_size == 0)
		{
			return;
		}

		//Allocate the instances GPU memory
		instances_gpu_allocation = manager->GetGPUMemoryRenderModule()->AllocStaticGPUMemory(manager->GetDevice(), gpu_allocation_size, nullptr, render::GetGameFrameIndex(manager->GetRenderSystem()));


		//Create the instance list GPU allocation and memory
		//The instance list is a count and  a list of offset to each instance in the tile log group
		size_t round_size = render::RoundSizeUp16Bytes((num_box_instances + 1) * sizeof(uint32_t));
		std::vector<uint32_t> instance_list_offsets(round_size / sizeof(uint32_t));
		instance_list_offsets[0] = static_cast<uint32_t>(num_box_instances); //First is the count
		size_t begin_instance_offset = manager->GetGPUMemoryRenderModule()->GetStaticGPUMemoryOffset(instances_gpu_allocation);
		for (size_t i = 0; i < num_box_instances; ++i)
		{
			instance_list_offsets[i + 1] = static_cast<uint32_t>(begin_instance_offset + i * sizeof(GPUBoxInstance));
		}
		instance_list_gpu_allocation = manager->GetGPUMemoryRenderModule()->AllocStaticGPUMemory(manager->GetDevice(), round_size, instance_list_offsets.data(), render::GetGameFrameIndex(manager->GetRenderSystem()));

		//Each instance it will reserve like a lineal allocator
		uint32_t gpu_offset = 0;

		//First dynamic
		for (auto& building_data : lod_group_data.animated_building_data)
		{
			//Create Box list
			render::AllocHandle box_list_handle = CreateBoxList(manager, building_data.m_boxes);
			//GPU memory
			GPUBoxInstance gpu_box_instance;
			gpu_box_instance.Fill(building_data.oob_box.position, building_data.oob_box.extents, glm::toQuat(building_data.oob_box.rotation), building_data.oob_box.position, glm::toQuat(building_data.oob_box.rotation), static_cast<uint32_t>(manager->GetGPUMemoryRenderModule()->GetStaticGPUMemoryOffset(box_list_handle)));

			//Update the GPU memory
			manager->GetGPUMemoryRenderModule()->UpdateStaticGPUMemory(manager->GetDevice(), instances_gpu_allocation, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(manager->GetRenderSystem()), gpu_offset * sizeof(GPUBoxInstance));

			InterpolatedPosition interpolated_position;
			interpolated_position.position.Reset(building_data.oob_box.position);

			//Calculate range AABB
			helpers::AABB range_aabb;
			helpers::OBB range_obb = building_data.oob_box;
			range_obb.extents.z += building_data.animation.range;
			helpers::CalculateAABBFromOBB(range_aabb, range_obb);

			//Just make it static
			instances_vector.push_back(ecs::AllocInstance<GameDatabase, AnimatedBoxType>(m_zone_id)
				.Init<OBBBox>(building_data.oob_box)
				.Init<RangeAABB>(range_aabb)
				.Init<AnimationBox>(building_data.animation)
				.Init<BoxGPUHandle>(gpu_offset, static_cast<uint32_t>(lod_group))
				.Init<BoxListHandle>(std::move(box_list_handle))
				.Init<InterpolatedPosition>(interpolated_position)
				.Init<LastPosition>(interpolated_position.position.Last()));

			gpu_offset++;
			COUNTER_INC(c_BuildingInstances_Count);
		}

		for (auto& building_data : lod_group_data.building_data)
		{
			//Create Box list
			render::AllocHandle box_list_handle = CreateBoxList(manager, building_data.m_boxes);

			//GPU memory
			GPUBoxInstance gpu_box_instance;
			gpu_box_instance.Fill(building_data.oob_box.position, building_data.oob_box.extents, glm::toQuat(building_data.oob_box.rotation), building_data.oob_box.position, glm::toQuat(building_data.oob_box.rotation), static_cast<uint32_t>(manager->GetGPUMemoryRenderModule()->GetStaticGPUMemoryOffset(box_list_handle)));

			//Update the GPU memory
			manager->GetGPUMemoryRenderModule()->UpdateStaticGPUMemory(manager->GetDevice(), instances_gpu_allocation, &gpu_box_instance, sizeof(GPUBoxInstance), render::GetGameFrameIndex(manager->GetRenderSystem()), gpu_offset * sizeof(GPUBoxInstance));

			//Calculate range aabb
			helpers::AABB range_aabb;
			helpers::CalculateAABBFromOBB(range_aabb, building_data.oob_box);

			//Just make it static
			instances_vector.push_back(ecs::AllocInstance<GameDatabase, BoxType>(m_zone_id)
				.Init<OBBBox>(building_data.oob_box)
				.Init<RangeAABB>(range_aabb)
				.Init<BoxListHandle>(std::move(box_list_handle))
				.Init<BoxGPUHandle>(gpu_offset, static_cast<uint32_t>(lod_group)));

			gpu_offset++;
			COUNTER_INC(c_BuildingInstances_Count);
		}
	}

	void Tile::SpawnTile(Manager* manager, uint32_t lod)
	{
		//LOD 0 has Rest, TopBuildings and TopPanels
		//LOD 1 has TopBuildings and Top Panels
		//LOD 2 has TopBuildings only
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
				COUNTER_SUB(c_BuildingInstances_Count);
			}

			//Set to invalid to the GPU memory
			instance.Get<BoxGPUHandle>().offset_gpu_allocator = BoxGPUHandle::kInvalidOffset;

			//Deallocate the box list
			manager->GetGPUMemoryRenderModule()->DeallocStaticGPUMemory(manager->GetDevice(), instance.Get<BoxListHandle>().box_list_handle, render::GetGameFrameIndex(manager->GetRenderSystem()));

			//Dealloc instance
			ecs::DeallocInstance<GameDatabase>(instance);
		}
		GetLodInstances(lod_group).clear();

		//Dealloc the gpu handles
		if (GetLodInstancesGPUAllocation(lod_group).IsValid())
		{
			manager->GetGPUMemoryRenderModule()->DeallocStaticGPUMemory(manager->GetDevice(), GetLodInstancesGPUAllocation(lod_group), render::GetGameFrameIndex(manager->GetRenderSystem()));
		}
		if (GetLodInstanceListGPUAllocation(lod_group).IsValid())
		{
			manager->GetGPUMemoryRenderModule()->DeallocStaticGPUMemory(manager->GetDevice(), GetLodInstanceListGPUAllocation(lod_group), render::GetGameFrameIndex(manager->GetRenderSystem()));
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

			if (m_lod == 0)
			{
				//We need to build the bvh buildings
				LinearBVHBuildingSettings settings;
				std::vector<InstanceReference> building_instances;
				building_instances.reserve(m_generated_boxes.size());

				for (auto& instances_lod_group : m_instances)
				{
					for (auto& instance : instances_lod_group.instances)
					{
						if (instance.Is<BoxType>() || instance.Is<AnimatedBoxType>())
						{
							building_instances.push_back(instance);
						}
					}
				}
				m_building_bvh.Build(&settings, building_instances.data(), static_cast<uint32_t>(building_instances.size()), m_bounding_box);
			}
			else
			{
				m_building_bvh.Clear();
			}
		}
	}
	void Tile::AppendVisibleInstanceLists(Manager* manager, std::vector<uint32_t>& instance_lists_offsets_array)
	{
		for (auto& lod_group : m_instances)
		{
			if (lod_group.m_instance_list_gpu_allocation.IsValid())
			{
				size_t instance_list_offset = manager->GetGPUMemoryRenderModule()->GetStaticGPUMemoryOffset(lod_group.m_instance_list_gpu_allocation);
				instance_lists_offsets_array.push_back(static_cast<uint32_t>(instance_list_offset));

				COUNTER_INC_VALUE(c_Building_Summitted, static_cast<uint32_t>(lod_group.instances.size()));
			}
		}
	}
	void Tile::AddedToLoadingQueue()
	{
		assert(m_state == State::Unloaded || m_state == State::Loaded);
		SetState(State::Loading);
	}
	glm::vec3 Tile::GetTrafficTargetPosition(uint32_t i, uint32_t j, uint32_t k) const
	{
		return m_traffic_targets[i + j * 2 + k * 4].position;
	}
	glm::vec3 Tile::GetTrafficNextTargetPosition(uint32_t i, uint32_t j, uint32_t k, uint32_t random) const
	{
		return m_traffic_targets[i + j * 2 + k * 4].next_position[random % 6];
	}
}