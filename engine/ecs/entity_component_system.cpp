#include "entity_component_system.h"

namespace ecs
{
	//Container with components for each instance type
	struct InstanceTypeContainer
	{

	};

	//Database
	//Our Database will have a list of instance types
	//Instance types depends of the combination of components added to a instance
	//Each instance type will have a size and a set of memory blocks with all compoments memory
	struct Database
	{
		//List of components
		std::vector<Component> m_components;

		//List of all instance types
		std::vector<InstanceTypeContainer> m_instance_types;
	};

	Database* CreateDatabase(const DatabaseDesc & database_desc)
	{
		Database* database = new Database();

		//Get all components information
		database->m_components = database_desc.components;

		//Reserve a buffer of instance types
		//We want to avoid reallocations during filling up of the database
		database->m_instance_types.reserve(database_desc.instance_type_reserve_size);

		return database;
	}

	void DestroyDatabase(Database*& database)
	{
		assert(database);
		delete database;
		database = nullptr;
	}
}