#include "entity_component_system.h"

namespace ecs
{
	//Container with components for each entity type
	struct EntityTypeContainer
	{
		EntityTypeMask m_mask;
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
		//List of components
		std::vector<Component> m_components;

		//List of all entity types
		std::vector<EntityTypeContainer> m_entity_types;

		//List of indirection instance indexes
		std::vector<InternalInstanceIndex> m_indirection_instance_table;
	};

	namespace internal
	{
		Database* CreateDatabase(const DatabaseDesc & database_desc)
		{
			Database* database = new Database();

			//Get all components information
			database->m_components = database_desc.components;

			//Add all entity types registered
			database->m_entity_types.resize(database_desc.entity_types.size());
			for (size_t i = 0; i < database_desc.entity_types.size(); ++i)
			{
				database->m_entity_types[i].m_mask = database_desc.entity_types[i];
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
	}
}