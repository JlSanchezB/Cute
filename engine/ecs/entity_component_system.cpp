#include "entity_component_system.h"

namespace ecs
{
	//Container with components for each entity type
	struct EntityTypeContainer
	{
		uint64_t m_mask;
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
	};

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

		return database;
	}

	void DestroyDatabase(Database*& database)
	{
		assert(database);
		delete database;
		database = nullptr;
	}
}