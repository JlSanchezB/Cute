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

	constexpr static size_t kTileDimension = 5;

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
};

#endif //BOX_CITY_TILE_MANAGER_H