#ifndef BOX_CITY_TILE_MANAGER_H
#define BOX_CITY_TILE_MANAGER_H

#include "box_city_components.h"
#include "box_city_tile.h"
#include <helpers/collision.h>
#include <bitset>
#include <queue>
#include <condition_variable>

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

	//World Sizes
	constexpr float kTileSize = 1000.f;
	constexpr float kTileHeightTop = 250.f;
	constexpr float kTileHeightBottom = -1000.f;
	constexpr float kTileHeightTopViewRange = 200.f;
	
	constexpr uint32_t kNumZonesXY = 100;
	constexpr uint32_t kNumZonesZ = 3;
	constexpr float kZoneWorldSizeXY = 80000.f;
	constexpr float kZoneWorldSizeZ = kTileHeightTop - kTileHeightBottom;
	constexpr float kZoneXYvsZRatio = (kZoneWorldSizeXY / kNumZonesXY) / (kZoneWorldSizeZ / kNumZonesZ);

	//Get local tiles index from world tiles
	inline LocalTilePosition CalculateLocalTileIndex(const WorldTilePosition& world_tile_position)
	{
		//First we need to move them in the position range
		//Then we do a modulo

		return LocalTilePosition{ static_cast<uint32_t>((world_tile_position.i + kLocalTileCount * 10000)) % kLocalTileCount, static_cast<uint32_t>((world_tile_position.j + kLocalTileCount * 10000)) % kLocalTileCount };
	}

	inline WorldTilePosition CalculateWorldPositionToWorldTile(const glm::vec3& position)
	{
		return WorldTilePosition{ static_cast<int32_t>(floor((position.x / kTileSize))), static_cast<int32_t>(floor((position.y / kTileSize))) };
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

		//Return a descriptor index for a position, if there is not a descriptor, it is just a gap
		std::optional<uint32_t> GetZoneDescriptorIndex(const glm::vec3& position);

		template<typename VISITOR>
		void VisitTiles(const helpers::AABB& box, VISITOR&& visitor)
		{
			WorldTilePosition min_tile = CalculateWorldPositionToWorldTile(box.min);
			WorldTilePosition max_tile = CalculateWorldPositionToWorldTile(box.max);

			auto clamp = [](int32_t x, int32_t min, int32_t max) -> int32_t
			{
				if (x < min) x = min;
				else if (x > max) x = max;
				return x;
			};

			//Because the tiles can overlap a little, we need to check the neighbourn tiles
			min_tile.i = clamp(min_tile.i - 1, m_camera_tile_position.i - kLocalTileCount / 2, m_camera_tile_position.i + kLocalTileCount / 2);
			min_tile.j = clamp(min_tile.j - 1, m_camera_tile_position.j - kLocalTileCount / 2, m_camera_tile_position.j + kLocalTileCount / 2);
			max_tile.i = clamp(max_tile.i + 1, m_camera_tile_position.i - kLocalTileCount / 2, m_camera_tile_position.i + kLocalTileCount / 2);
			max_tile.j = clamp(max_tile.j + 1, m_camera_tile_position.j - kLocalTileCount / 2, m_camera_tile_position.j + kLocalTileCount / 2);

			//Loop to all tiles
			for (int32_t ii = min_tile.i; ii <= max_tile.i; ++ii)
				for (int32_t jj = min_tile.j; jj <= max_tile.j; ++jj)
				{
					WorldTilePosition it{ ii, jj };
					Tile& tile = GetTile(CalculateLocalTileIndex(it));
					//Check the collision
					if (tile.IsVisible() && helpers::CollisionAABBVsAABB(tile.GetBoundingBox(), box))
					{
						visitor(tile);
					}
				}

		}

		//Visit buildings bvh
		template<typename VISITOR>
		void VisitBuildings(const helpers::AABB& box, VISITOR&& visitor)
		{
			//Loop to all tiles around the box
			VisitTiles(box, [&](Tile& tile)
				{
					tile.GetBuildingsBVH().Visit(box, [&](const InstanceReference& instance)
						{
							//call our visitor with all the instances
							visitor(instance);
						});
				});
		}
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

		//Zone distribution
		void GenerateZoneDescriptors();
		
		//Zone distributions
		struct ZoneDescriptor
		{
			//position inside the zone
			glm::vec3 position;
			uint32_t descriptor_index;
		};
		std::vector<ZoneDescriptor> m_descriptor_zones;

		//Loading system
		struct LoadingJob
		{
			Tile* tile;
			LocalTilePosition local_tile_position;
			WorldTilePosition world_tile_position;
		};

		std::unique_ptr<core::Thread> m_loading_thread;
		core::Mutex m_loading_access_mutex;
		std::queue<LoadingJob> m_loading_queue;
		std::condition_variable m_loading_queue_condition_variable;
		bool m_loading_thread_quit = false;
		std::atomic_bool m_tiles_loaded = false;

		//Add Tile to Load
		void AddTileToLoad(Tile& tile, const LocalTilePosition& local_tile_position, const WorldTilePosition& world_tile_position);

		//Loading Thread Run access
		static void LoadingThreadRun(Manager* manager);
	};
}

#endif //BOX_CITY_TILE_MANAGER_H