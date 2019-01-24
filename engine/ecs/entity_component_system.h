//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system interface
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_SYSTEM_H_
#define ENTITY_COMPONENT_SYSTEM_H_

#include <vector>
#include <cassert>

namespace ecs
{
	struct Database;

	using EntityTypeMask = uint64_t;
	using InstanceIndirectionIndexType = uint32_t;

	template<typename ...COMPONENTS>
	struct EntityType
	{
		//Return the mask that represent this instance type, mask is a bit set with the components enabled
		static EntityTypeMask GetEntityTypeMask()
		{
			return ((1 << ComponentDesc<COMPONENTS>::s_component_index) | ...);
		}

		inline static size_t s_instance_type_index = static_cast<size_t>(-1);
	};

	//Templated instance class, needs to be specialized in the client
	template<typename DATABASE_NAME>
	struct Instance
	{
		//All instances will able to access the database associated to them
		inline static Database* s_database;

		InstanceIndirectionIndexType indirection_index;
	};

	//Represent all information needed for the ECS about the component
	struct Component
	{
		size_t size;
		size_t align;

		//Move operator
		void(*move_operator)(void*, void*);

		//Capture the properties of the component
		template<typename COMPONENT>
		void Capture()
		{
			size = sizeof(COMPONENT);
			align = alignof(COMPONENT);

			move_operator = ComponentDesc<COMPONENT>::Move;
		}
	};

	struct DatabaseDesc
	{
		//List of components
		std::vector<Component> components;

		//List of register entity types
		std::vector<EntityTypeMask> entity_types;

		//Number of zones
		size_t num_zones = 1;

		//Number max of entities per zone
		size_t num_max_entities_zone = 1024;

		//Add component
		template<typename COMPONENT>
		void AddComponent()
		{
			//Component class can be used only once and only in one Database
			assert(ComponentDesc<COMPONENT>::s_component_index == static_cast<size_t>(-1));

			Component component;
			//Capture the information needed
			component.Capture<COMPONENT>();
			//Added to the component list
			components.push_back(component);

			//Set the index to the component index, so the type of the component can be converted to the index inside component database
			ComponentDesc<COMPONENT>::s_component_index = components.size() - 1;
		}

		//Add entity type
		template<typename ENTITY_TYPE>
		void AddEntityType()
		{
			assert(ENTITY_TYPE::s_instance_type_index == static_cast<size_t>(-1));

			entity_types.push_back(ENTITY_TYPE::GetEntityTypeMask());

			ENTITY_TYPE::s_instance_type_index = entity_types.size() - 1;
		}
	};

	namespace internal
	{
		//Create database from a database description with the component lists
		Database* CreateDatabase(const DatabaseDesc& database_desc);
		void DestroyDatabase(Database*& database);

		//Alloc instance
		InstanceIndirectionIndexType AllocInstance(Database* database, const size_t& entity_type_index);
	}

	//Create database from a database description with the component lists
	template<typename DATABASE_NAME>
	Database* CreateDatabase(const DatabaseDesc& database_desc)
	{
		//Create the database
		Database* database = internal::CreateDatabase(database_desc);
		//Set the static fast access for the instances with this DATABASE_NAME
		Instance<DATABASE_NAME>::s_database = database;

		return database;
	}
	template<typename DATABASE_NAME>
	void DestroyDatabase(Database*& database)
	{
		internal::DestroyDatabase(database);

		//Reset fast access 
		Instance<DATABASE_NAME>::s_database = nullptr;
	}

	//Alloc instance
	template<typename DATABASE_NAME, typename EntityType>
	Instance<DATABASE_NAME> AllocInstance()
	{
		Instance<DATABASE_NAME> instance;
		instance.indirection_index = internal::AllocInstance(Instance<DATABASE_NAME>::s_database, EntityType::s_instance_type_index);

		return instance;
	}
}

#endif //ENTITY_COMPONENT_SYSTEM_H_
