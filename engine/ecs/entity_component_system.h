//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system interface
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_SYSTEM_H_
#define ENTITY_COMPONENT_SYSTEM_H_

#include <vector>
#include <cassert>
#include <core/type_list.h>

namespace ecs
{
	struct Database;

	using EntityTypeMask = uint64_t;
	using InstanceIndirectionIndexType = uint32_t;

	template<typename ...COMPONENTS>
	struct EntityType
	{
		//Return the mask that represent this instance type, mask is a bit set with the components enabled
		template<typename DATABASE_DECLARATION>
		constexpr static EntityTypeMask EntityTypeMask()
		{
			return ((1ul << DATABASE_DECLARATION::Components::template ElementIndex<ComponentDesc<COMPONENTS>>()) | ...);
		}
	};

	//Templated instance class, needs to be specialized in the client
	template<typename DATABASE_DECLARATION>
	class Instance
	{
	public:
		InstanceIndirectionIndexType indirection_index;
	};


	//Represent the database
	template<typename COMPONENT_LIST, typename ENTITY_TYPE_LIST>
	struct DatabaseDeclaration
	{
		//All instances will able to access the database associated to them
		inline static Database* s_database;

		using Components = COMPONENT_LIST;
		using EntityTypes = ENTITY_TYPE_LIST;

		template<typename COMPONENT>
		constexpr size_t ComponentIndex()
		{
			return COMPONENT_LIST::template ElementIndex<COMPONENT>();
		}
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
			size = sizeof(typename COMPONENT::Type);
			align = alignof(typename COMPONENT::Type);

			move_operator = COMPONENT::Move;
		}
	};

	struct DatabaseDesc
	{
		//Number of zones
		size_t num_zones = 1;

		//Number max of entities per zone
		size_t num_max_entities_zone = 1024;
	};

	namespace internal
	{
		//Create database from a database description with the component lists
		Database* CreateDatabase(const DatabaseDesc& database_desc, const std::vector<Component>& components, const std::vector<EntityTypeMask> entity_type_masks);
		void DestroyDatabase(Database*& database);

		//Alloc instance
		InstanceIndirectionIndexType AllocInstance(Database* database, const size_t& entity_type_index);

		//Dealloc instance
		void DeallocInstance(Database* database, InstanceIndirectionIndexType index);

		//Get component data
		void* GetComponentData(Database* database, InstanceIndirectionIndexType index, size_t component_index);

	}

	//Helpers to extract all the components
	struct FillComponents
	{
		inline static std::vector<Component>* container;
		template<typename COMPONENT>
		static void Visit()
		{
			Component component;
			//Capture the information needed
			component.Capture<COMPONENT>();
			//Added to the component list
			container->push_back(component);
		}
	};

	//Helpers to extract all the entity types
	template<typename DATABASE_DECLARATION>
	struct FillEntityTypes
	{
		inline static std::vector<EntityTypeMask>* container;
		template<typename ENTITY_TYPE>
		static void Visit()
		{
			container->push_back(ENTITY_TYPE::template EntityTypeMask<DATABASE_DECLARATION>());
		}
	};

	//Create database from a database description with the component lists
	template<typename DATABASE_DECLARATION>
	Database* CreateDatabase(const DatabaseDesc& database_desc)
	{
		//List of components using type_list visit
		std::vector<Component> components;
		FillComponents::container = &components;
		DATABASE_DECLARATION::Components::template Visit<FillComponents>();

		//List of register entity types using type_list visit
		std::vector<EntityTypeMask> entity_types;
		FillEntityTypes<DATABASE_DECLARATION>::container = &entity_types;
		DATABASE_DECLARATION::EntityTypes::template Visit<FillEntityTypes<DATABASE_DECLARATION>>();

		//Create the database
		Database* database = internal::CreateDatabase(database_desc, components, entity_types);
		//Set the static fast access for the instances with this DATABASE_NAME
		DATABASE_DECLARATION::s_database = database;

		return database;
	}
	template<typename DATABASE_DECLARATION>
	void DestroyDatabase()
	{
		internal::DestroyDatabase(DatabaseDeclaration<DATABASE_DECLARATION>::s_database);
	}

	//Alloc instance
	template<typename DATABASE_DECLARATION, typename ENTITY_TYPE>
	Instance<DATABASE_DECLARATION> AllocInstance()
	{
		Instance<DATABASE_DECLARATION> instance;
		instance.indirection_index = internal::AllocInstance(DATABASE_DECLARATION::s_database, DATABASE_DECLARATION::EntityTypes::template ElementIndex<ENTITY_TYPE>());

		return instance;
	}
}

#endif //ENTITY_COMPONENT_SYSTEM_H_
