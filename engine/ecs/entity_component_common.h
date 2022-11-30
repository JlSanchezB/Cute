//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system interface
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_SYSTEM_COMMON_H_
#define ENTITY_COMPONENT_SYSTEM_COMMON_H_

namespace ecs
{
	struct Database;

	using EntityTypeMask = uint64_t;

	struct InstanceIndirectionIndexType
	{
		uint32_t thread_id : 8;
		uint32_t index : 24;

		static constexpr uint32_t kInvalidThreadID = 0xFF;
		static constexpr uint32_t kInvalidIndex = 0xFFFFFF;
	};


	using ComponentType = uint8_t;
	using ZoneType = uint16_t;
	using EntityTypeType = uint16_t;
	using InstanceIndexType = uint32_t;

	struct DatabaseStats
	{
		size_t num_deferred_deletions;
		size_t num_deferred_moves;
	};

	namespace internal
	{
		//Get instance type mask from a indirection index
		EntityTypeMask GetInstanceTypeMask(Database* database, InstanceIndirectionIndexType index);

		//Get instance type mask from a entity type index
		EntityTypeMask GetInstanceTypeMask(Database* database, EntityTypeType entity_type);

		//Get instance type index from a entity type index
		size_t GetInstanceTypeIndex(Database* database, InstanceIndirectionIndexType index);

		//Get component data
		void* GetComponentData(Database* database, InstanceIndirectionIndexType index, ComponentType component_index);
	}
}

#endif
