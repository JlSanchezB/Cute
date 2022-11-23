#ifndef BOX_CITY_TRAFFIC_MANAGER_H
#define BOX_CITY_TRAFFIC_MANAGER_H

#include "box_city_components.h"
#include <bitset>
#include "box_city_tile_manager.h"

namespace render
{
	class GPUMemoryRenderModule;
}

namespace BoxCityTrafficSystem
{
	//Number of cars per tile
	constexpr uint32_t kNumCars = 2000;
	
	//Number of tiles, we have a lot less than the city manager
	constexpr uint32_t kLocalTileCount = 5;

	//World size
	constexpr float kTileSize = 1000.f;

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
		//Init
		void Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module, const glm::vec3& camera_position);

		//Shutdown
		void Shutdown();

		//Update
		void Update(const glm::vec3& camera_position);

		//GPU Access
		render::AllocHandle& GetGPUHandle()
		{
			return m_gpu_memory;
		}

		std::bitset<BoxCityTileSystem::kLocalTileCount* BoxCityTileSystem::kLocalTileCount> GetCameraBitSet(const helpers::Frustum& frustum) const
		{
			std::bitset<BoxCityTileSystem::kLocalTileCount* BoxCityTileSystem::kLocalTileCount> ret(false);

			for (auto& tile : m_tiles)
			{
				if (tile.m_activated)
				{
					ret[tile.m_zone_index] = helpers::CollisionFrustumVsAABB(frustum, tile.m_bounding_box);
				}
			}

			return ret;
		}

	private:
		//Systems
		display::Device* m_device = nullptr;
		render::System* m_render_system = nullptr;
		render::GPUMemoryRenderModule* m_GPU_memory_render_module = nullptr;

		struct TileDescriptor
		{
			int32_t i_offset; //offset from the center
			int32_t j_offset; //offset from the center
		};

		//The tile descriptors are sorted from center
		std::vector<TileDescriptor> m_tile_descriptors;

		//Zone distribution
		void GenerateZoneDescriptors();

		struct Tile
		{
			bool m_activated = false;

			//Zone index
			uint32_t m_zone_index;

			//World time index that represent
			WorldTilePosition m_tile_position{ std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min()};

			//Bounding box of the tile
			helpers::AABB m_bounding_box;
		};

		//Tiles
		Tile m_tiles[kLocalTileCount * kLocalTileCount];

		//Get tile
		Tile& GetTile(const LocalTilePosition& local_tile);

		//SetupCar
		void SetupCar(Tile& tile, std::mt19937& random, float begin_tile_x, float begin_tile_y,
			std::uniform_real_distribution<float>& position_range, std::uniform_real_distribution<float>& position_range_z, std::uniform_real_distribution<float>& size_range,
			Car& car, CarSettings& car_settings, OBBBox& obb_component, AABBBox& aabb_component, CarGPUIndex& car_gpu_index);

		//Current camera tile position, center of our local tiles
		WorldTilePosition m_camera_tile_position;

		//GPU memory
		render::AllocHandle m_gpu_memory;

		//Free GPU slots
		std::vector<uint16_t> m_free_gpu_slots;

		uint16_t AllocGPUSlot();
		void DeallocGPUSlot(uint16_t slot);
	};
}

#endif //BOX_CITY_TRAFFIC_MANAGER_H