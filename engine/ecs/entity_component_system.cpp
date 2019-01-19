#include "entity_component_system.h"

namespace ecs
{
	//Database
	//Our Database will have a list of instance types
	//Instance types depends of the combination of components added to a instance
	//Each instance type will have a size and a set of memory blocks with all compoments memory
	struct Database
	{
	};

	Database* CreateDatabase(const DatabaseDesc & database_desc)
	{
		return nullptr;
	}

	void DestroyDatabase(Database * database)
	{
	}
}