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
	
	//Represent a local tile position
	struct LocalTilePosition
	{
		uint32_t i;
		uint32_t j;
	};

	//Represents a world tile position
	struct WorldTilePosition
	{
		int32_t i;
		int32_t j;
	};

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
		WorldTilePosition tile_position;

		//Load?
		bool load = false;

		//Vector of all the bbox already in the tile
		std::vector<BoxCollision> m_generated_boxes;
	};



#ifndef _DEBUG
	//Needs to be odd number, as the camera is in the middle, when same tiles left and right
	constexpr static size_t kLocalTileSize = 11;
#else
	constexpr static size_t kLocalTileSize = 3;
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
			if (tile.load)
			{
				ret[tile.zone_id] = helpers::CollisionFrustumVsAABB(frustum, tile.bounding_box);
			}
		}

		return ret;
	}

	//Get local tiles index from world tiles
	LocalTilePosition CalculateLocalTileIndex(const WorldTilePosition& world_tile_position)
	{
		//First we need to move them in the position range
		//Then we do a modulo

		return LocalTilePosition{ static_cast<uint32_t>((world_tile_position.i + kLocalTileSize * 10000)) % kLocalTileSize, static_cast<uint32_t>((world_tile_position.j + kLocalTileSize * 10000)) % kLocalTileSize };
	}

	//Get world tile position from a position
	WorldTilePosition GetWorldTilePosition(const glm::vec3& position)
	{
		return WorldTilePosition{static_cast<int32_t>(position.x / kLocalTileSize), static_cast<int32_t>(position.y / kLocalTileSize) };
	}

	//Init
	void Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);

	//Update, it will check if new tiles need to be created/move because the camera has moved
	void Update(const glm::vec3& camera_position);

private:
	//System
	display::Device* m_device = nullptr;
	render::System* m_render_system = nullptr;
	render::GPUMemoryRenderModule* m_GPU_memory_render_module = nullptr;

	//Tiles
	Tile m_tiles[kLocalTileSize * kLocalTileSize];

	//Current camera tile position, center of our local tiles
	WorldTilePosition m_camera_tile_position;
	
	Tile& GetTile(const LocalTilePosition& local_tile);
	void BuildTile(const LocalTilePosition& local_tile, const WorldTilePosition& world_tile);
	void BuildBlock(std::mt19937& random, const uint16_t zone_id, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box);
	
};

#endif //BOX_CITY_TILE_MANAGER_H