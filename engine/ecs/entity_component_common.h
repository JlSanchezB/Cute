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
}

#endif
