#include "box_city_tile_manager.h"
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

void BoxCityTileManager::Build(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module)
{
	for (size_t i_tile = 0; i_tile < BoxCityTileManager::kTileDimension; ++i_tile)
	{
		for (size_t j_tile = 0; j_tile < BoxCityTileManager::kTileDimension; ++j_tile)
		{
			BuildTile(i_tile, j_tile, device, render_system, GPU_memory_render_module);
		}
	}
}

void BoxCityTileManager::BuildTile(const size_t i_tile, const size_t j_tile, display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module)
{
	std::mt19937 random(static_cast<uint32_t>(i_tile + j_tile * BoxCityTileManager::kTileDimension));

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
		helpers::OBB extended_obb_box = obb_box;
		if (dynamic_box)
		{
			extended_obb_box.extents.z += animated_box.range;
		}
		helpers::AABB extended_aabb_box;
		helpers::CalculateAABBFromOBB(extended_aabb_box, extended_obb_box);


		//First collision in the tile
		bool collide = CollisionBoxVsTile(extended_aabb_box, extended_obb_box, tile.m_generated_boxes);


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
					collide = CollisionBoxVsTile(extended_aabb_box, extended_obb_box, GetTile(ii, jj).m_generated_boxes);
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
			tile.m_generated_boxes.push_back({ extended_aabb_box, extended_obb_box });
		}

		//Block can be build
		BuildBlock(random, tile.zone_id, obb_box, aabb_box, dynamic_box, animated_box, device, render_system, GPU_memory_render_module);

		//Gow zone AABB by the bounding box
		tile.bounding_box.min = glm::min(tile.bounding_box.min, extended_aabb_box.min);
		tile.bounding_box.max = glm::max(tile.bounding_box.max, extended_aabb_box.max);
	}

}

void BoxCityTileManager::BuildBlock(std::mt19937& random, const uint16_t zone_id, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box, display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module)
{
	//Just a little smaller, so it has space for the panels
	const float panel_depth = 0.1f;

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
	render::AllocHandle gpu_memory = GPU_memory_render_module->AllocStaticGPUMemory(device, sizeof(GPUBoxInstance), &gpu_box_instance, render::GetGameFrameIndex(render_system));

	ecs::InstanceReference box_reference;
	if (!dynamic_box)
	{
		//Just make it static
		box_reference = ecs::AllocInstance<GameDatabase, BoxType>(zone_id)
			.Init<OBBBox>(oob_box_component)
			.Init<AABBBox>(aabb_box_component)
			.Init<BoxRender>(box_render)
			.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) });
	}
	else
	{
		box_reference = ecs::AllocInstance<GameDatabase, AnimatedBoxType>(zone_id)
			.Init<OBBBox>(oob_box_component)
			.Init<AABBBox>(aabb_box_component)
			.Init<BoxRender>(box_render)
			.Init<AnimationBox>(animated_box)
			.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) });
	}

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

		std::uniform_real_distribution<float> panel_size_range(0.2f, glm::min(wall_width, 1.f));

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
			render::AllocHandle gpu_memory = GPU_memory_render_module->AllocStaticGPUMemory(device, sizeof(GPUBoxInstance), &gpu_box_instance, render::GetGameFrameIndex(render_system));

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
				ecs::AllocInstance<GameDatabase, AttachedPanelType>(zone_id)
					.Init<OBBBox>(panel_obb)
					.Init<AABBBox>(panel_aabb)
					.Init<BoxRender>(box_render)
					.Init<Attachment>(attachment)
					.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory) });
			}
			else
			{
				//Add without attachment
				ecs::AllocInstance<GameDatabase, PanelType>(zone_id)
					.Init<OBBBox>(panel_obb)
					.Init<AABBBox>(panel_aabb)
					.Init<BoxRender>(box_render)
					.Init<BoxGPUHandle>(BoxGPUHandle{ std::move(gpu_memory)});
			}
		}
	}
	
}
