#ifndef BOX_CITY_TILE_H
#define BOX_CITY_TILE_H

#include "box_city_components.h"
#include <helpers/collision.h>
#include <helpers/bvh.h>

namespace render
{
	class GPUMemoryRenderModule;
}

namespace BoxCityTileSystem
{
	class Manager;

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
		TopBuildings,
		TopPanels,
		Rest,
		Count
	};

	constexpr static uint32_t kNumLods = 3;
	//LOD 0 has Rest, TopBuildings and TopPanels
	//LOD 1 has TopBuildings and Top Panels
	//LOD 2 has TopBuildings only

	//Mask to represent each lod
	constexpr static uint32_t kLodMask[kNumLods] =
	{
		1 << static_cast<uint32_t>(LODGroup::Rest) | 1 << static_cast<uint32_t>(LODGroup::TopBuildings) | 1 << static_cast<uint32_t>(LODGroup::TopPanels),
		1 << static_cast<uint32_t>(LODGroup::TopBuildings) | 1 << static_cast<uint32_t>(LODGroup::TopPanels),
		1 << static_cast<uint32_t>(LODGroup::TopBuildings)
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
	public:
		LODGroupData& GetLodGroupData(const LODGroup lod_group)
		{
			return m_level_data[static_cast<size_t>(lod_group)];
		}

		auto& GetLodInstances(const LODGroup lod_group)
		{
			return m_instances[static_cast<size_t>(lod_group)];
		}

		render::AllocHandle& GetLodGPUAllocation(const LODGroup lod_group)
		{
			return m_gpu_allocation[static_cast<size_t>(lod_group)];
		}

		bool CollisionBoxVsLoadedTile(const helpers::AABB& aabb_box, const helpers::OBB& obb_box) const;
		bool CollisionBoxVsLoadingTile(const helpers::AABB& aabb_box, const helpers::OBB& obb_box) const;

		//Building logic
		void BuildTileData(Manager* manager, const LocalTilePosition& local_tile, const WorldTilePosition& world_tile);
		void BuildBlockData(std::mt19937& random, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box, const uint32_t descriptor_index = 0);

		//Spawn/Despawn logic
		void SpawnLodGroup(Manager* manager, const LODGroup lod_group);
		void SpawnTile(Manager* manager, uint32_t lod);
		void DespawnLodGroup(Manager* manager, const LODGroup lod_group);
		void DespawnTile(Manager* manager);
		void LodTile(Manager* manager, uint32_t new_lod);

		bool IsVisible() const { return m_state == State::Visible;}
		bool IsLoaded() const {	return m_state == State::Loaded || m_state == State::Visible;}
		bool IsLoading() const { return m_state == State::Loading;}
		uint32_t CurrentLod() const { return m_lod;}
		uint32_t GetZoneID() const { return m_zone_id;}
		helpers::AABB GetBoundingBox() const { return m_bounding_box;}
		WorldTilePosition GetWorldTilePosition() const { return m_tile_position;}

		//Call to indicate that has been added in the queue for loading
		void AddedToLoadingQueue();
	private:
		enum class State
		{
			Unloaded,
			Loading,
			Loaded,
			Visible
		};

		std::atomic<State> m_state = State::Unloaded;

		constexpr static uint16_t kInvalidTile = static_cast<uint16_t>(-1);

		helpers::AABB m_bounding_box;
		uint16_t m_zone_id = kInvalidTile;

		//World time index that represent
		WorldTilePosition m_tile_position;

		//Current lod
		uint32_t m_lod = 0;

		//Vector of all the bbox in the tile
		std::vector<BoxCollision> m_generated_boxes;

		//LBVH for generated boxes, it will help a lot for building the neighbours
		struct LinearBVHSettings
		{
			using IndexType = uint32_t;
			static void SetLeafIndex(const uint32_t index) {};
			static helpers::AABB GetAABB(const BoxCollision& box_collision) { return box_collision.aabb; }
		};
		helpers::LinearBVH<BoxCollision, LinearBVHSettings> m_generated_boxes_bvh;

		//Vector of precalculated items
		std::array<LODGroupData, static_cast<size_t>(LODGroup::Count)> m_level_data;

		//Vector of the block instances m_loaded in the ECS
		std::array<std::vector<Instance>, static_cast<size_t>(LODGroup::Count)> m_instances;

		//Vector of the GPU allocation for each lod group
		std::array<render::AllocHandle, static_cast<size_t>(LODGroup::Count)> m_gpu_allocation;

		void SetState(State new_state)
		{
			m_state = new_state;
		}
	};
};

#endif //BOX_CITY_TILE_H