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

	enum class LODGroup
	{
		TopBuildings, //Top Buildings, used for LOD 0,1,2
		TopPanels, //Top pannels, Used for LOD 0,1 
		Rest, // Rest, used for LOD 0
		Count
	};

	struct BoxData
	{
		helpers::AABB aabb_box;
		helpers::OBB oob_box;
	};

	struct AnimatedBoxData : public BoxData
	{
		AnimationBox animation;
	};

	struct PanelData : BoxData
	{
		uint8_t colour_palette;
	};

	struct AnimatedPanelData : PanelData
	{
		uint32_t parent_index;
		glm::mat4x4 parent_to_child;
	};

	struct LODGroupData
	{
		std::vector<BoxData> building_data;
		std::vector<PanelData> panel_data;
		std::vector<AnimatedBoxData> animated_building_data;
		std::vector<AnimatedPanelData> animated_panel_data;

		void clear()
		{
			building_data.clear();
			panel_data.clear();
			animated_building_data.clear();
			animated_panel_data.clear();
		}
	};

	struct Tile
	{
		constexpr static uint16_t kInvalidTile = static_cast<uint16_t>(-1);

		helpers::AABB bounding_box;
		uint16_t zone_id = kInvalidTile;

		//World time index that represent
		WorldTilePosition tile_position;

		//Load?
		bool loaded = false;

		//Visible
		bool visible = false;

		//Current lod
		uint32_t lod = 0;

		//Vector of all the bbox in the tile
		std::vector<BoxCollision> generated_boxes;

		//Vector of precalculated items
		std::array<LODGroupData, static_cast<size_t>(LODGroup::Count)> level_data;

		//Vector of the block instances loaded in the ECS
		std::array<std::vector<Instance>, static_cast<size_t>(LODGroup::Count)> instances;

		//Vector of the GPU allocation for each lod group
		std::array<render::AllocHandle, static_cast<size_t>(LODGroup::Count)> gpu_allocation;

		LODGroupData& GetLodGroupData(const LODGroup lod_group)
		{
			return level_data[static_cast<size_t>(lod_group)];
		}

		auto& GetLodInstances(const LODGroup lod_group)
		{
			return instances[static_cast<size_t>(lod_group)];
		}

		render::AllocHandle& GetLodGPUAllocation(const LODGroup lod_group)
		{
			return gpu_allocation[static_cast<size_t>(lod_group)];
		}
	};



#ifndef _DEBUG
	//Needs to be odd number, as the camera is in the middle, when same tiles left and right
	constexpr static uint32_t kLocalTileCount = 15;
#else
	constexpr static uint32_t kLocalTileCount = 3;
#endif

	constexpr static float kTileSize = 1000.f;
	constexpr static float kTileHeightTop = 250.f;
	constexpr static float kTileHeightBottom = -250.f;

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
			if (tile.visible)
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

	//Shutdown
	void Shutdown();

	//Update, it will check if new tiles need to be created/move because the camera has moved
	void Update(const glm::vec3& camera_position);

	//Get GPU alloc handle from zoneID
	render::AllocHandle& GetGPUHandle(uint32_t zoneID, uint32_t lod_group);
private:
	//System
	display::Device* m_device = nullptr;
	render::System* m_render_system = nullptr;
	render::GPUMemoryRenderModule* m_GPU_memory_render_module = nullptr;

	struct TileDescriptor
	{
		bool loaded; //tiles outside the radius will get unloaded
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
	std::array<Tile,kLocalTileCount * kLocalTileCount> m_tiles;

	//Current camera tile position, center of our local tiles
	WorldTilePosition m_camera_tile_position;

	//Needs more streaming to do
	bool m_pending_streaming_work = false;

	void GenerateTileDescriptors();
	
	Tile& GetTile(const LocalTilePosition& local_tile);

	//Builds the tile data, creates the tile
	void BuildTileData(const LocalTilePosition& local_tile, const WorldTilePosition& world_tile);
	void BuildBlockData(std::mt19937& random, Tile& tile, const uint16_t zone_id, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box);

	//Spawn lod group
	void SpawnLodGroup(Tile& tile, const LODGroup lod_group);
	//Spawn the tile in the ECS
	void SpawnTile(Tile& tile, uint32_t lod);
	//Despawn lod group
	void DespawnLodGroup(Tile& tile, const LODGroup lod_group);
	//Despawn the tile from the ECS
	void DespawnTile(Tile& tile);
	//Change lod
	void LodTile(Tile& tile, uint32_t new_lod);
};

#endif //BOX_CITY_TILE_MANAGER_H