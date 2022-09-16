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

		//World time index that represent
		int32_t world_i;
		int32_t world_j;

		//Load?
		bool load = false;

		//Vector of all the bbox already in the tile
		std::vector<BoxCollision> m_generated_boxes;
	};

#ifndef _DEBUG
	constexpr static size_t kLocalTileSize = 12;
#else
	constexpr static size_t kLocalTileSize = 2;
#endif

	constexpr static float kTileSize = 500.f;
	constexpr static float kTileHeightTop = 250.f;
	constexpr static float kTileHeightBottom = -650.f;

	Tile& GetTile(size_t i, size_t j)
	{
		return m_tiles[i + j * kLocalTileSize];
	}

	constexpr static size_t GetNumTiles()
	{
		return kLocalTileSize * kLocalTileSize;
	}

	std::bitset<kLocalTileSize * kLocalTileSize> GetAllZoneBitSet() const
	{
		return std::bitset<kLocalTileSize * kLocalTileSize>(true);
	}

	std::bitset<kLocalTileSize * kLocalTileSize> GetCameraBitSet(const helpers::Frustum& frustum) const
	{
		std::bitset<kLocalTileSize * kLocalTileSize> ret(false);

		for (auto& tile : m_tiles)
		{
			ret[tile.zone_id] = helpers::CollisionFrustumVsAABB(frustum, tile.bounding_box);
		}

		return ret;
	}

	//Get local tiles index from world tiles
	std::pair<size_t, size_t> CalculateLocalTileIndex(const int32_t i, const int32_t j)
	{
		//First we need to move them in the position range
		//Then we do a modulo

		return std::make_pair(static_cast<size_t>((j + kLocalTileSize * 10000))% kLocalTileSize, static_cast<size_t>((j + kLocalTileSize * 10000)) % kLocalTileSize);
	}

	//Builds the start tiles
	void Build(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);

	//Update, it will check if new tiles need to be created/move because the camera has moved
	void Update(const glm::vec3& camera_position);

private:
	Tile m_tiles[kLocalTileSize * kLocalTileSize];

	void BuildTile(const size_t i_tile, const size_t j_tile, display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);
	void BuildBlock(std::mt19937& random, const uint16_t zone_id, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box, display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);
};

#endif //BOX_CITY_TILE_MANAGER_H