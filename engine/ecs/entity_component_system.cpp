#include "entity_component_system.h"
#include <core/virtual_buffer.h>

namespace ecs
{
	//Container with components for each entity type
	struct EntityTypeContainer
	{
		EntityTypeMask m_mask;

		//Each component is a virtual buffer (some of them inited as zero)
		std::vector<core::VirtualBuffer> m_components;
	};

	//Each zone has a list of entity type containers
	struct ZoneContainer
	{
		std::vector<EntityTypeContainer> m_entity_types;
	};

	//Represent a instance (in one zone, one instance type and the index)
	struct InternalInstanceIndex
	{
		uint16_t zone_index;
		uint16_t entity_type_index;
		uint32_t instance_index;
	};

	//Database
	//Our Database will have a list of instance types
	//Instance types depends of the combination of components added to a instance
	//Each instance type will have a size and a set of memory blocks with all compoments memory
	struct Database
	{
		//Fast access data
		size_t m_num_components;
		size_t m_num_entity_types;
		size_t m_num_zones;

		//List of components
		std::vector<Component> m_components;

		//List of all zones
		std::vector<ZoneContainer> m_zones;

		//List of indirection instance indexes
		std::vector<InternalInstanceIndex> m_indirection_instance_table;
	};

	namespace internal
	{
		Database* CreateDatabase(const DatabaseDesc & database_desc, const std::vector<Component>& components, const std::vector<EntityTypeMask> entity_types)
		{
			assert(database_desc.num_zones > 0);
			assert(database_desc.num_zones < std::numeric_limits<uint16_t>::max());
			assert(entity_types.size() < std::numeric_limits<uint16_t>::max());
			assert(components.size() < 64);

			Database* database = new Database();

			//Init sizes
			database->m_num_zones = database_desc.num_zones;
			database->m_num_components = components.size();
			database->m_num_entity_types = entity_types.size();

			//Get all components information
			database->m_components = components;

			//Create all the zones
			database->m_zones.resize(database_desc.num_zones);

			//For each zone add the entity types
			for (auto& zone : database->m_zones)
			{
				//Add all entity types registered
				zone.m_entity_types.resize(entity_types.size());
				for (size_t i = 0; i < entity_types.size(); ++i)
				{
					auto& entity_type = zone.m_entity_types[i];

					entity_type.m_mask = entity_types[i];

					//Create all components needed
					entity_type.m_components.reserve(database->m_num_components);

					//Create all virtual buffers for each component
					for (size_t j = 0; j < database->m_num_components; ++j)
					{
						const size_t compoment_buffer_size = database_desc.num_max_entities_zone * database->m_components[j].size;
						entity_type.m_components.emplace_back((((1ULL << j) & entity_type.m_mask) != 0 ) ? compoment_buffer_size : 0);
					}
				}
			}
			
			//Reserve memory for the indirection indexes
			database->m_indirection_instance_table.reserve(1024);

			return database;
		}

		void DestroyDatabase(Database*& database)
		{
			assert(database);
			delete database;
			database = nullptr;
		}

		InstanceIndirectionIndexType AllocInstance(Database * database, const size_t & entity_type_index)
		{
			//Allocate indirection index
			database->m_indirection_instance_table.emplace_back(InternalInstanceIndex({ 0,0,0 }));
			assert(database->m_indirection_instance_table.size() < std::numeric_limits<InstanceIndirectionIndexType>::max());
			
			return static_cast<InstanceIndirectionIndexType>(database->m_indirection_instance_table.size() - 1);
		}
		void DeallocInstance(Database * database, InstanceIndirectionIndexType index)
		{
		}
		void * GetComponentData(Database * database, InstanceIndirectionIndexType index, size_t component_index)
		{
			return nullptr;
		}
	}
}