#ifndef BOX_CITY_TILE_MANAGER_H
#define BOX_CITY_TILE_MANAGER_H

#include "box_city_components.h"
#include "box_city_tile.h"
#include <helpers/collision.h>
#include <bitset>

namespace render
{
	class GPUMemoryRenderModule;
}

namespace BoxCityTileSystem
{
#ifndef _DEBUG
	//Needs to be odd number, as the camera is in the middle, when same tiles left and right
	constexpr uint32_t kLocalTileCount = 15;
#else
	constexpr uint32_t kLocalTileCount = 3;
#endif

	constexpr float kTileSize = 1000.f;
	constexpr float kTileHeightTop = 250.f;
	constexpr float kTileHeightBottom = -250.f;
	constexpr float kTileHeightTopViewRange = 200.f;
	
	//Get local tiles index from world tiles
	inline LocalTilePosition CalculateLocalTileIndex(const WorldTilePosition& world_tile_position)
	{
		//First we need to move them in the position range
		//Then we do a modulo

		return LocalTilePosition{ static_cast<uint32_t>((world_tile_position.i + kLocalTileCount * 10000)) % kLocalTileCount, static_cast<uint32_t>((world_tile_position.j + kLocalTileCount * 10000)) % kLocalTileCount };
	}

	class Manager
	{
	public:
		Tile& GetTile(size_t i, size_t j)
		{
			return m_tiles[i + j * kLocalTileCount];
		}

		constexpr static size_t GetNumTiles()
		{
			return kLocalTileCount * kLocalTileCount;
		}

		std::bitset<kLocalTileCount* kLocalTileCount> GetAllZoneBitSet() const
		{
			return std::bitset<kLocalTileCount* kLocalTileCount>(true);
		}

		std::bitset<kLocalTileCount* kLocalTileCount> GetCameraBitSet(const helpers::Frustum& frustum) const
		{
			std::bitset<kLocalTileCount* kLocalTileCount> ret(false);

			for (auto& tile : m_tiles)
			{
				if (tile.IsVisible())
				{
					ret[tile.GetZoneID()] = helpers::CollisionFrustumVsAABB(frustum, tile.GetBoundingBox());
				}
			}

			return ret;
		}

		//Init
		void Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);

		//Shutdown
		void Shutdown();

		//Update, it will check if new tiles need to be created/move because the camera has moved
		void Update(const glm::vec3& camera_position);

		//Get GPU alloc handle from zoneID
		render::AllocHandle& GetGPUHandle(uint32_t zoneID, uint32_t lod_group);

		display::Device* GetDevice() { return m_device; };
		render::System* GetRenderSystem() { return m_render_system; };
		render::GPUMemoryRenderModule* GetGPUMemoryRenderModule() { return m_GPU_memory_render_module; };

	private:
		//System
		display::Device* m_device = nullptr;
		render::System* m_render_system = nullptr;
		render::GPUMemoryRenderModule* m_GPU_memory_render_module = nullptr;

		struct TileDescriptor
		{
			bool m_loaded; //tiles outside the radius will get unloaded
			uint32_t lod; //lod index of this tile, it depends of the center distance
			int32_t i_offset; //offset from the center
			int32_t j_offset; //offset from the center
			uint32_t index; //index in the kLocalTileCount * kLocalTileCount vector
			float normalized_distance;
			float distance;
		};

		//The tile descriptors are sorted from center
		std::vector<TileDescriptor> m_tile_descriptors;

		//Tiles
		std::array<Tile, kLocalTileCount* kLocalTileCount> m_tiles;

		//Current camera tile position, center of our local tiles
		WorldTilePosition m_camera_tile_position;

		//Current camera height position
		bool m_camera_top_range = false;

		//Needs more streaming to do
		bool m_pending_streaming_work = false;

		void GenerateTileDescriptors();

		Tile& GetTile(const LocalTilePosition& local_tile);
	};
}

#endif //BOX_CITY_TILE_MANAGER_H