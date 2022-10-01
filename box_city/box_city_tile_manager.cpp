#include "box_city_tile_manager.h"
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <render/render.h>
#include <render_module/render_module_gpu_memory.h>
#include <core/counters.h>

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

void BoxCityTileManager::Update(const glm::vec3& camera_position)
{
	//Check if the camera is still in the same tile, using some fugde factor
	constexpr float fudge_factor = 0.05f;
	float min_x = (-0.5f + m_camera_tile_position.i - fudge_factor) * kTileSize;
	float min_y = (-0.5f + m_camera_tile_position.j - fudge_factor) * kTileSize;
	float max_x = (-0.5f + m_camera_tile_position.i + 1 + fudge_factor) * kTileSize;
	float max_y = (-0.5f + m_camera_tile_position.j + 1 + fudge_factor) * kTileSize;

	//If the camera has move of tile, destroy the tiles out of view and create the new ones
	bool camera_moved = (camera_position.x < min_x) || (camera_position.y < min_y) || (camera_position.x > max_x) || (camera_position.y > max_y);
	if (camera_moved || m_pending_streaming_work)
	{
		if (camera_moved)
		{
			m_camera_tile_position = WorldTilePosition{ static_cast<int32_t>(-0.5f + (camera_position.x / kTileSize)), static_cast<int32_t>(-0.5f + (camera_position.y / kTileSize)) };;
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

			//Check if the tile has the different world index
			if ((tile.tile_position.i != world_tile.i || tile.tile_position.j != world_tile.j) && tile.load || !tile_descriptor.loaded && tile.load)
			{
				//Dealloc blocks
				for (auto& block_instance : tile.block_instances)
				{
					//Dealloc the gpu handle
					m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, block_instance.Get<BoxGPUHandle>().gpu_memory, render::GetGameFrameIndex(m_render_system));
						
					//Dealloc instance
					ecs::DeallocInstance<GameDatabase>(block_instance);

					COUNTER_SUB(c_Blocks_Count);
				}
				tile.block_instances.clear();

				//Dealloc panels
				for (auto& panel_instance : tile.panel_instances)
				{
					//Dealloc the gpu handle
					m_GPU_memory_render_module->DeallocStaticGPUMemory(m_device, panel_instance.Get<BoxGPUHandle>().gpu_memory, render::GetGameFrameIndex(m_render_system));

					//Dealloc instance
					ecs::DeallocInstance<GameDatabase>(panel_instance);

					COUNTER_SUB(c_Panels_Count);
				}
				tile.panel_instances.clear();

				tile.load = false;
				num_tile_changed++;

				core::LogInfo("Tile Local<%i,%i>, World<%i,%i> unloaded", local_tile.i, local_tile.j, world_tile.i, world_tile.j);
			}

			//If tile is unloaded but it needs to be loaded
			if (!tile.load && tile_descriptor.loaded && num_tile_changed < max_tile_changed_per_frame)
			{
				//Create new time
				BuildTile(local_tile, world_tile);
				num_tile_changed++;

				core::LogInfo("Tile Local<%i,%i>, World<%i,%i> loaded", local_tile.i, local_tile.j, world_tile.i, world_tile.j);
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
				if (tile_descriptor.normalized_distance < 0.5f)
				{
					//0.0 to 0.5 is max lod, lod 0
					tile_descriptor.lod = 0;
				}
				else if (tile_descriptor.normalized_distance < 0.75f)
				{
					//0.5 to 0.75 is lod 1
					tile_descriptor.lod = 1;
				}
				else
				{
					//0.75 to 1.0 is lod 2
					tile_descriptor.lod = 2;
				}
			}
			else
			{
				//Non loaded
				tile_descriptor.loaded = false;
				tile_descriptor.lod = 0;
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

void BoxCityTileManager::BuildTile(const LocalTilePosition& local_tile, const WorldTilePosition& world_tile)
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
	tile.load = true;
	tile.tile_position = world_tile;
	tile.generated_boxes.clear();

	float high_range_cut = FLT_MIN;

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

		//Cut bottom boxes
		//if (obb_box.position.z < high_range_cut)
		//	continue;

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
					if (neighbour_tile.load)
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
		BuildBlock(random, tile, tile.zone_id, obb_box, aabb_box, dynamic_box, animated_box);

		//Gow zone AABB by the bounding box
		tile.bounding_box.min = glm::min(tile.bounding_box.min, extended_aabb_box.min);
		tile.bounding_box.max = glm::max(tile.bounding_box.max, extended_aabb_box.max);
	}

}

void BoxCityTileManager::BuildBlock(std::mt19937& random, Tile& tile, const uint16_t zone_id, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box)
{
	//Just a little smaller, so it has space for the panels
	const float panel_depth = 5.0f;

	OBBBox oob_box_component;
	oob_box_component.position = obb.position;
	oob_box_component.extents = obb.extents - glm::vec3(panel_depth, panel_depth, 0.f);
	oob_box_component.rotation = obb.rotation;

	AABBBox aabb_box_component;
	aabb_box_component.min = aabb.min;
	aabb_box_component.max = aabb.max;

	BoxRender box_render;
	box_render.colour = glm::vec4(1.f, 1.f, 1.f, 0.f);

	//GPU memory
	GPUBoxInstance gpu_box_instance;
	gpu_box_instance.Fill(oob_box_component);
	gpu_box_instance.Fill(box_render);

	//Allocate the GPU memory
	render::AllocHandle gpu_memory = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, sizeof(GPUBoxInstance), &gpu_box_instance, render::GetGameFrameIndex(m_render_system));

	if (!dynamic_box)
	{
		//Just make it static
		tile.block_instances.push_back(ecs::AllocInstance<GameDatabase, BoxType>(zone_id)
			.Init<OBBBox>(oob_box_component)
			.Init<AABBBox>(aabb_box_component)
			.Init<BoxRender>(box_render)
			.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) }));

		COUNTER_INC(c_Blocks_Count);
	}
	else
	{
		tile.block_instances.push_back(ecs::AllocInstance<GameDatabase, AnimatedBoxType>(zone_id)
			.Init<OBBBox>(oob_box_component)
			.Init<AABBBox>(aabb_box_component)
			.Init<BoxRender>(box_render)
			.Init<AnimationBox>(animated_box)
			.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) }));

		COUNTER_INC(c_Blocks_Count);
	}
	ecs::InstanceReference box_reference = tile.block_instances.back();

	//Create panels in each side of the box

	//Matrix used for the attachments
	glm::mat4x4 box_to_world(oob_box_component.rotation);
	box_to_world[3] = glm::vec4(oob_box_component.position, 1.f);

	std::vector<std::pair<glm::vec2, glm::vec2>> panels_generated;
	for (size_t face = 0; face < 4; ++face)
	{
		//For each face try to create panels
		const float wall_width = (face%2==0) ? oob_box_component.extents.x : oob_box_component.extents.y;
		const float wall_heigh = oob_box_component.extents.z;
		panels_generated.clear();

		std::uniform_real_distribution<float> panel_size_range(5.f, glm::min(wall_width, 15.f));

		//Calculate rotation matrix of the face and position
		glm::mat3x3 face_rotation = glm::mat3x3(glm::rotate(glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f))) * glm::mat3x3(glm::rotate(glm::half_pi<float>() * face, glm::vec3(0.f, 0.f, 1.f)))  * oob_box_component.rotation;
		glm::vec3 face_position = oob_box_component.position + glm::vec3(0.f, 0.f, wall_width) * face_rotation;
		
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

			//Color palette
			const glm::vec4 colour_palette[] =
			{
				{1.f, 0.1f, 0.6f, 0.f},
				{1.f, 0.6f, 0.1f, 0.f},
				{1.f, 0.95f, 0.f, 0.f},
				{0.5f, 1.f, 0.f, 0.f},
				{0.f, 1.0f, 1.f, 0.f}
			};

			//Calculate oob for the panel
			OBBBox panel_obb;
			panel_obb.position = face_position + glm::vec3(panel_position.x, panel_position.y, panel_depth / 2.f) * face_rotation;
			panel_obb.rotation = face_rotation;
			panel_obb.extents = glm::vec3(panel_size.x, panel_size.y, panel_depth / 2.f);

			AABBBox panel_aabb;
			helpers::CalculateAABBFromOBB(panel_aabb, panel_obb);

			BoxRender box_render;
			box_render.colour = colour_palette[random() % 5] * 2.f;

			//GPU memory
			GPUBoxInstance gpu_box_instance;
			gpu_box_instance.Fill(oob_box_component);
			gpu_box_instance.Fill(box_render);

			//Allocate the GPU memory
			render::AllocHandle gpu_memory = m_GPU_memory_render_module->AllocStaticGPUMemory(m_device, sizeof(GPUBoxInstance), &gpu_box_instance, render::GetGameFrameIndex(m_render_system));

			//Add
			if (dynamic_box)
			{
				//Calculate attachment matrix
				Attachment attachment;
				attachment.parent = box_reference;

				glm::mat4x4 panel_to_world(panel_obb.rotation);
				panel_to_world[3] = glm::vec4(panel_obb.position, 1.f);
				
				attachment.parent_to_child = glm::inverse(box_to_world) * panel_to_world;

				//Add attached
				tile.panel_instances.push_back(ecs::AllocInstance<GameDatabase, AttachedPanelType>(zone_id)
					.Init<OBBBox>(panel_obb)
					.Init<AABBBox>(panel_aabb)
					.Init<BoxRender>(box_render)
					.Init<Attachment>(attachment)
					.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) }));

				COUNTER_INC(c_Panels_Count);
			}
			else
			{
				//Add without attachment
				tile.panel_instances.push_back(ecs::AllocInstance<GameDatabase, PanelType>(zone_id)
					.Init<OBBBox>(panel_obb)
					.Init<AABBBox>(panel_aabb)
					.Init<BoxRender>(box_render)
					.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory)}));

				COUNTER_INC(c_Panels_Count);
			}
		}
	}
	
}
