#ifndef BOX_CITY_TRAFFIC_MANAGER_H
#define BOX_CITY_TRAFFIC_MANAGER_H

#include "box_city_components.h"

namespace render
{
	class GPUMemoryRenderModule;
}

namespace BoxCityTrafficSystem
{
	//Number of cars per tile
	constexpr uint32_t kNumCars = 500;
	
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

	private:
		//Systems
		display::Device* m_device = nullptr;
		render::System* m_render_system = nullptr;
		render::GPUMemoryRenderModule* m_GPU_memory_render_module = nullptr;

		struct TileDescriptor
		{
			bool active; //tiles outside the radius will get deactivated
			int32_t i_offset; //offset from the center
			int32_t j_offset; //offset from the center
			uint32_t index; //index in the kLocalTileCount * kLocalTileCount vector
		};

		//The tile descriptors are sorted from center
		std::vector<TileDescriptor> m_tile_descriptors;

		//Zone distribution
		void GenerateZoneDescriptors();

		struct Tile
		{
			bool m_activated = false;

			//World time index that represent
			WorldTilePosition m_tile_position;
		};

		//Tiles
		Tile m_tiles[kLocalTileCount * kLocalTileCount];

		//Get tile
		Tile& GetTile(const LocalTilePosition& local_tile);

		//Current camera tile position, center of our local tiles
		WorldTilePosition m_camera_tile_position;

	};
}

#endif //BOX_CITY_TRAFFIC_MANAGER_H