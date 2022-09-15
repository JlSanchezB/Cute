#ifndef BOX_CITY_TILE_MANAGER_H
#define BOX_CITY_TILE_MANAGER_H

#include "box_city_components.h"
#include <bitset>
#include <helpers/collision.h>
#include <bitset>

namespace render
{
	class GPUMemoryRenderModule;
}

struct BoxCityTileManager
{
public:
	struct BoxCollision
	{
		helpers::AABB aabb;
		helpers::OBB obb;
	};
	struct Tile
	{
		constexpr static uint16_t kInvalidTile = static_cast<uint16_t>(-1);

		helpers::AABB bounding_box;
		uint16_t zone_id = kInvalidTile;

		//Vector of all the bbox already in the tile
		std::vector<BoxCollision> m_generated_boxes;
	};

#ifndef _DEBUG
	constexpr static size_t kTileDimension = 12;
#else
	constexpr static size_t kTileDimension = 2;
#endif

	Tile& GetTile(size_t i, size_t j)
	{
		return m_tiles[i + j * kTileDimension];
	}

	constexpr static size_t GetNumTiles()
	{
		return kTileDimension * kTileDimension;
	}

	std::bitset<kTileDimension* kTileDimension> GetAllZoneBitSet() const
	{
		return std::bitset<kTileDimension* kTileDimension>(true);
	}

	std::bitset<kTileDimension* kTileDimension> GetCameraBitSet(const helpers::Frustum& frustum) const
	{
		std::bitset<kTileDimension* kTileDimension> ret(false);

		for (auto& tile : m_tiles)
		{
			ret[tile.zone_id] = helpers::CollisionFrustumVsAABB(frustum, tile.bounding_box);
		}

		return ret;
	}

	//Builds the start tiles
	void Build(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);

private:
	Tile m_tiles[kTileDimension * kTileDimension];

	void BuildTile(const size_t i_tile, const size_t j_tile, display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);
	void BuildBlock(std::mt19937& random, const uint16_t zone_id, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box, display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);
};

#endif //BOX_CITY_TILE_MANAGER_H