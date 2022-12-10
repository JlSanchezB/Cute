#ifndef BOX_CITY_TRAFFIC_MANAGER_H
#define BOX_CITY_TRAFFIC_MANAGER_H

#include "box_city_components.h"
#include <bitset>
#include "box_city_tile_manager.h"
#include <job/job.h>
#include <helpers/camera.h>

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

	inline WorldTilePosition CalculateWorldPositionToWorldTile(const glm::vec3& position)
	{
		return WorldTilePosition{ static_cast<int32_t>(floor((position.x / kTileSize))), static_cast<int32_t>(floor((position.y / kTileSize))) };
	}

	inline uint32_t CalculateLocalTileToZoneIndex(const LocalTilePosition& local_tile_position)
	{
		return local_tile_position.i + local_tile_position.j * kLocalTileCount;
	}

	class Manager
	{
	public:
		//Init
		void Init(display::Device* device, render::System* render_system, render::GPUMemoryRenderModule* GPU_memory_render_module);

		//Shutdown
		void Shutdown();

		//Update
		void Update(const glm::vec3& camera_position);

		//Update Cars
		void UpdateCars(platform::Game* game, job::System* job_system, job::JobAllocator<1024 * 1024>* job_allocator, const helpers::Camera& camera, job::Fence& update_fence, BoxCityTileSystem::Manager* tile_manager, uint32_t frame_index, float elapsed_time);

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

		InstanceReference GetPlayerCar()
		{
			return m_player_car;
		}

		void SetPlayerControlEnable(bool enable)
		{
			m_player_control_enable = enable;
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

		//Player car
		InstanceReference m_player_car;

		//Player control 
		bool m_player_control_enable = false;

		//Tiles
		Tile m_tiles[kLocalTileCount * kLocalTileCount];

		//Get tile
		Tile& GetTile(const LocalTilePosition& local_tile);
		Tile& GetTile(const uint32_t& zone_index)
		{
			return m_tiles[zone_index];
		}

		//SetupCar
		void SetupCar(Tile& tile, std::mt19937& random, float begin_tile_x, float begin_tile_y,
			std::uniform_real_distribution<float>& position_range, std::uniform_real_distribution<float>& position_range_z, std::uniform_real_distribution<float>& size_range,
			Car& car, CarMovement& car_movement, CarSettings& car_settings, OBBBox& obb_component, AABBBox& aabb_component, CarGPUIndex& car_gpu_index);

		void SetupCarTarget(std::mt19937& random, Car& car, CarTarget& car_target);

		//Current camera tile position, center of our local tiles
		WorldTilePosition m_camera_tile_position;

		//GPU memory
		render::AllocHandle m_gpu_memory;
	};
}

#endif //BOX_CITY_TRAFFIC_MANAGER_H