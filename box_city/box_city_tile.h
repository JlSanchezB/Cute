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
		glm::vec3 position;
		glm::vec3 extents;
		uint8_t colour_palette;
	};

	struct InstanceData
	{
		helpers::OBB oob_box;
		std::vector<BoxData> m_boxes; //Include the building itself
	};

	struct AnimatedInstanceData : public InstanceData
	{
		AnimationBox animation;

		AnimatedInstanceData(const InstanceData&& instance_data)
		{
			oob_box = instance_data.oob_box;
			m_boxes = std::move(instance_data.m_boxes);
		}
	};

	struct LODGroupData
	{
		std::vector<InstanceData> building_data;
		std::vector<AnimatedInstanceData> animated_building_data;

		void clear()
		{
			building_data.clear();
			animated_building_data.clear();
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
			return m_instances[static_cast<size_t>(lod_group)].instances;
		}

		render::AllocHandle& GetLodInstancesGPUAllocation(const LODGroup lod_group)
		{
			return m_instances[static_cast<size_t>(lod_group)].m_instances_gpu_allocation;
		}

		render::AllocHandle& GetLodInstanceListGPUAllocation(const LODGroup lod_group)
		{
			return m_instances[static_cast<size_t>(lod_group)].m_instance_list_gpu_allocation;
		}

		bool CollisionBoxVsLoadedTile(const helpers::AABB& aabb_box, const helpers::OBB& obb_box) const;
		bool CollisionBoxVsLoadingTile(const helpers::AABB& aabb_box, const helpers::OBB& obb_box) const;

		//Building logic
		void BuildTileData(Manager* manager, const LocalTilePosition& local_tile, const WorldTilePosition& world_tile);
		void BuildBlockData(std::mt19937& random, const helpers::OBB& obb, helpers::AABB& aabb, const bool dynamic_box, const AnimationBox& animated_box, const uint32_t descriptor_index = 0);
		render::AllocHandle CreateBoxList(Manager* manager, const std::vector<BoxData>& box_data);

		//Spawn/Despawn logic
		void SpawnLodGroup(Manager* manager, const LODGroup lod_group);
		void SpawnTile(Manager* manager, uint32_t lod);
		void DespawnLodGroup(Manager* manager, const LODGroup lod_group);
		void DespawnTile(Manager* manager);
		void LodTile(Manager* manager, uint32_t new_lod);

		void AppendVisibleInstanceLists(Manager* manager, std::vector<uint32_t>& instance_lists_offsets_array);

		bool IsVisible() const { return m_state == State::Visible;}
		bool IsLoaded() const {	return m_state == State::Loaded || m_state == State::Visible;}
		bool IsLoading() const { return m_state == State::Loading;}
		uint32_t CurrentLod() const { return m_lod;}
		uint32_t GetZoneID() const { return m_zone_id;}
		helpers::AABB GetBoundingBox() const { return m_bounding_box;}
		WorldTilePosition GetWorldTilePosition() const { return m_tile_position;}

		struct LinearBVHBuildingSettings
		{
			using IndexType = uint32_t;
			void SetLeafIndex(InstanceReference& instance, uint32_t) {}
			helpers::AABB GetAABB(const InstanceReference& instance) { return instance.Get<GameDatabase>().Get<RangeAABB>(); }
		};
		helpers::LinearBVH<InstanceReference, LinearBVHBuildingSettings>& GetBuildingsBVH()
		{
			return m_building_bvh;
		}

		//Call to indicate that has been added in the queue for loading
		void AddedToLoadingQueue();

		glm::vec3 GetTrafficTargetPosition(uint32_t i, uint32_t j, uint32_t k) const;
		glm::vec3 GetTrafficNextTargetPosition(uint32_t i, uint32_t j, uint32_t k, uint32_t random) const;
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
		struct LinearBVHGeneratedBoxesSettings
		{
			std::vector<BoxCollision>& generated_boxes;
			using IndexType = uint32_t;
			void SetLeafIndex(uint32_t, uint32_t)	{}
			helpers::AABB GetAABB(const uint32_t& index) { return generated_boxes[index].aabb;}

			LinearBVHGeneratedBoxesSettings(std::vector<BoxCollision>& _generated_boxes): generated_boxes(_generated_boxes) {}
		};
		helpers::LinearBVH<uint32_t, LinearBVHGeneratedBoxesSettings> m_generated_boxes_bvh;

		//Vector of precalculated items
		std::array<LODGroupData, static_cast<size_t>(LODGroup::Count)> m_level_data;

		struct LODGroupInstances
		{
			std::vector<Instance> instances;
			render::AllocHandle m_instances_gpu_allocation;
			render::AllocHandle m_instance_list_gpu_allocation;
		};
		//Vector of the block instances m_loaded in the ECS
		std::array<LODGroupInstances, static_cast<size_t>(LODGroup::Count)> m_instances;

		//LBVH for building instances
		helpers::LinearBVH<InstanceReference, LinearBVHBuildingSettings> m_building_bvh;

		//Each tile has 16 target positions
		struct Target
		{
			glm::vec3 position;
			std::array<glm::vec3, 6> next_position;
		};
		std::array<Target, 2 * 2 * 4> m_traffic_targets;

		void SetState(State new_state)
		{
			m_state = new_state;
		}
	};
};

#endif //BOX_CITY_TILE_H