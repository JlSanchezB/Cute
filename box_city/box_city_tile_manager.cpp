#include "box_city_tile_manager.h"
#include <random>
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <render/render.h>
#include <render_module/render_module_gpu_memory.h>

namespace
{
	bool CollisionBoxVsTile(const helpers::AABB& aabb_box, const helpers::OBB& obb_box, const std::vector<BoxCityTileManager::BoxCollision>& generated_boxes)
	{
		for (auto& current : generated_boxes)
		{
			//Collide
			if (helpers::CollisionAABBVsAABB(current.aabb, aabb_box) && helpers::CollisionOBBVsOBB(current.obb, obb_box))
			{
				return true;
			}
		}
		return false;
	}
}

void BoxCityTileManager::Build(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module)
{
	std::mt19937 random(34234234);

	constexpr float tile_dimension = 40.f;
	constexpr float tile_height_min = -40.f;
	constexpr float tile_height_max = 40.f;


	std::uniform_real_distribution<float> position_range(0, tile_dimension);
	std::uniform_real_distribution<float> position_range_z(tile_height_min, tile_height_max);
	std::uniform_real_distribution<float> angle_inc_range(-glm::half_pi<float>() * 0.2f, glm::half_pi<float>() * 0.2f);
	std::uniform_real_distribution<float> angle_rotation_range(0.f, glm::two_pi<float>());
	std::uniform_real_distribution<float> length_range(4.f, 12.f);
	std::uniform_real_distribution<float> size_range(1.5f, 2.5f);

	std::uniform_real_distribution<float> range_animation_range(0.f, 5.f);
	std::uniform_real_distribution<float> frecuency_animation_range(0.3f, 1.f);
	std::uniform_real_distribution<float> offset_animation_range(0.f, 10.f);

	const float static_range_box_city = 2.f;

	for (size_t i_tile = 0; i_tile < BoxCityTileManager::kTileDimension; ++i_tile)
		for (size_t j_tile = 0; j_tile < BoxCityTileManager::kTileDimension; ++j_tile)
		{
			//Tile positions
			const float begin_tile_x = i_tile * tile_dimension;
			const float begin_tile_y = j_tile * tile_dimension;

			BoxCityTileManager::Tile& tile = GetTile(i_tile, j_tile);

			tile.bounding_box.min = glm::vec3(begin_tile_x, begin_tile_y, tile_height_min);
			tile.bounding_box.max = glm::vec3(begin_tile_x + tile_dimension, begin_tile_y + tile_dimension, tile_height_max);

			tile.zone_id = static_cast<uint16_t>(i_tile + j_tile * BoxCityTileManager::kTileDimension);

			//Create boxes
			for (size_t i = 0; i < 150; ++i)
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
				helpers::OBB new_obb_box = obb_box;
				if (dynamic_box)
				{
					new_obb_box.extents.z += animated_box.range;
				}
				helpers::AABB new_aabb_box;
				helpers::CalculateAABBFromOBB(new_aabb_box, new_obb_box);


				//First collision in the tile
				bool collide = CollisionBoxVsTile(new_aabb_box, new_obb_box, tile.m_generated_boxes);


				size_t begin_i = (i_tile == 0) ? 0 : i_tile - 1;
				size_t end_i = std::min(BoxCityTileManager::kTileDimension - 1, i_tile + 1);

				size_t begin_j = (j_tile == 0) ? 0 : j_tile - 1;
				size_t end_j = std::min(BoxCityTileManager::kTileDimension - 1, j_tile + 1);

				//Then neighbours
				for (size_t ii = begin_i; (ii <= end_i) && !collide; ++ii)
				{
					for (size_t jj = begin_j; (jj <= end_j) && !collide; ++jj)
					{
						if (i_tile != ii || j_tile != jj)
						{
							collide = CollisionBoxVsTile(new_aabb_box, new_obb_box, tile.m_generated_boxes);
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
					tile.m_generated_boxes.push_back({ new_aabb_box, new_obb_box });
				}

				OBBBox oob_box_component;
				oob_box_component.position = obb_box.position;
				oob_box_component.extents = obb_box.extents;
				oob_box_component.rotation = obb_box.rotation;

				AABBBox aabb_box_component;
				aabb_box_component.min = aabb_box.min;
				aabb_box_component.max = aabb_box.max;

				//GPU memory
				GPUBoxInstance gpu_box_instance;
				gpu_box_instance.Fill(oob_box_component);

				//Allocate the GPU memory
				render::AllocHandle gpu_memory = GPU_memory_render_module->AllocStaticGPUMemory(device, sizeof(GPUBoxInstance), &gpu_box_instance, render::GetGameFrameIndex(render_system));

				//Gow zone AABB by the bounding box
				helpers::AABB extended_aabb;
				helpers::CalculateAABBFromOBB(extended_aabb, new_obb_box);
				tile.bounding_box.min = glm::min(tile.bounding_box.min, extended_aabb.min);
				tile.bounding_box.max = glm::max(tile.bounding_box.max, extended_aabb.max);

				if (!dynamic_box)
				{
					//Just make it static
					ecs::AllocInstance<GameDatabase, BoxType>(tile.zone_id)
						.Init<OBBBox>(oob_box_component)
						.Init<AABBBox>(aabb_box_component)
						.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) });
				}
				else
				{

					ecs::AllocInstance<GameDatabase, AnimatedBoxType>(tile.zone_id)
						.Init<OBBBox>(oob_box_component)
						.Init<AABBBox>(aabb_box_component)
						.Init<AnimationBox>(animated_box)
						.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) });
				}
			}
		}
}
