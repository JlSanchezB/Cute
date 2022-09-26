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
		std::vector<BoxCollision> generated_boxes;

		//Vector of the block instances
		std::vector<Instance> block_instances;
		std::vector<Instance> panel_instances;
	};



#ifndef _DEBUG
	//Needs to be odd number, as the camera is in the middle, when same tiles left and right
	constexpr static size_t kLocalTileCount = 11;
#else
	constexpr static size_t kLocalTileCount = 3;
#endif

	constexpr static float kTileSize = 500.f;
	constexpr static float kTileHeightTop = 250.f;
	constexpr static float kTileHeightBottom = -650.f;

	Tile& GetTile(size_t i, size_t j)
	{
		return m_tiles[i + j * kLocalTileCount];
	}

	constexpr static size_t GetNumTiles()
	{
		return kLocalTileCount * kLocalTileCount;
	}

	std::bitset<kLocalTileCount * kLocalTileCount> GetAllZoneBitSet() const
	{
		return std::bitset<kLocalTileCount * kLocalTileCount>(true);
	}

	std::bitset<kLocalTileCount * kLocalTileCount> GetCameraBitSet(const helpers::Frustum& frustum) const
	{
		std::bitset<kLocalTileCount * kLocalTileCount> ret(false);

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

		return LocalTilePosition{ static_cast<uint32_t>((world_tile_position.i + kLocalTileCount * 10000)) % kLocalTileCount, static_cast<uint32_t>((world_tile_position.j + kLocalTileCount * 10000)) % kLocalTileCount };
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
	Tile m_tiles[kLocalTileCount * kLocalTileCount];

	//Current camera tile position, center of our local tiles
	WorldTilePosition m_camera_tile_position;

	//Needs more streaming to do
	bool m_pending_streaming_work = false;

	
	Tile& GetTile(const LocalTilePosition& local_tile);
	void BuildTile(const LocalTilePosition& local_tile, const WorldTilePosition& world_tile);
	void BuildBlock(std::mt19937& random, Tile& tile, const uint16_t zone_id, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box);
	
};

#endif //BOX_CITY_TILE_MANAGER_H