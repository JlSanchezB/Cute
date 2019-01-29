//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system interface
//////////////////////////////////////////////////////////////////////////

namespace ecs
{
	struct Database;

	using EntityTypeMask = uint64_t;
	using InstanceIndirectionIndexType = uint32_t;

	using ComponentType = uint8_t;
	using ZoneType = uint16_t;
	using EntityTypeType = uint16_t;
	using InstanceIndexType = uint32_t;
}
