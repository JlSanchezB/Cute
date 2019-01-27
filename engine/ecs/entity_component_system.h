//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system interface
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_SYSTEM_H_
#define ENTITY_COMPONENT_SYSTEM_H_

#include <vector>
#include <cassert>
#include <core/type_list.h>
#include "entity_component_common.h"
#include "entity_component_instance.h"

namespace ecs
{
	template<typename ...COMPONENTS>
	struct EntityType
	{
		//Return the mask that represent this instance type, mask is a bit set with the components enabled
		template<typename DATABASE_DECLARATION>
		constexpr static EntityTypeMask EntityTypeMask()
		{
			return ((1ul << DATABASE_DECLARATION::Components::template ElementIndex<COMPONENTS>()) | ...);
		}
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
		constexpr static ComponentType ComponentIndex()
		{
			return static_cast<ComponentType>(COMPONENT_LIST::template ElementIndex<COMPONENT>());
		}

		template<typename ENTITY_TYPE>
		constexpr static EntityTypeType EntityTypeIndex()
		{
			return static_cast<EntityTypeType>(ENTITY_TYPE_LIST::template ElementIndex<ENTITY_TYPE>());
		}

		template<typename COMPONENT>
		constexpr static EntityTypeMask ComponentMask()
		{
			return (1ULL << ComponentIndex<COMPONENT>());
		}

		template<typename ENTITY_TYPE>
		constexpr static EntityTypeMask EntityTypeMask()
		{
			return ENTITY_TYPE_LIST::template EntityTypeMask<ENTITY_TYPE>();
		}
	};

	template<typename COMPONENT>
	struct ComponentMoveDeclaration
	{
		static void Move(void* ptr_a, void* ptr_b)
		{
			COMPONENT& a = *(reinterpret_cast<COMPONENT*>(ptr_a));
			COMPONENT& b = *(reinterpret_cast<COMPONENT*>(ptr_b));

			a = std::move(b);
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
			size = sizeof(COMPONENT);
			align = alignof(COMPONENT);

			move_operator = ComponentMoveDeclaration<COMPONENT>::Move;
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
		InstanceIndirectionIndexType AllocInstance(Database* database, ZoneType zone_index, EntityTypeType entity_type_index);

		//Dealloc instance
		void DeallocInstance(Database* database, InstanceIndirectionIndexType index);

		//Get component data
		void* GetComponentData(Database* database, InstanceIndirectionIndexType index, ComponentType component_index);

		//Get instance type from a indirection index
		size_t GetInstanceType(Database* database, InstanceIndirectionIndexType index);

		//Get instance type mask from a indirection index
		EntityTypeMask GetInstanceTypeMask(Database* database, InstanceIndirectionIndexType index);
	}

	//Create database from a database description with the component lists
	template<typename DATABASE_DECLARATION>
	Database* CreateDatabase(const DatabaseDesc& database_desc)
	{
		//List of components using type_list visit
		std::vector<Component> components;

		core::visit<DATABASE_DECLARATION::Components::template Size()>([&](auto component_index)
		{
			Component component;
			//Capture the information needed
			component.Capture<DATABASE_DECLARATION::Components::template ElementType<component_index.value>>();
			//Added to the component list
			components.push_back(component);
		});

		//List of register entity types using type_list visit
		std::vector<EntityTypeMask> entity_types;
		core::visit<DATABASE_DECLARATION::EntityTypes::template Size()>([&](auto entity_type_index)
		{
			using EntityTypeIt = typename DATABASE_DECLARATION::EntityTypes::template ElementType<entity_type_index.value>;
			entity_types.push_back(EntityTypeIt::template EntityTypeMask<DATABASE_DECLARATION>());
		});

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
	Instance<DATABASE_DECLARATION> AllocInstance(ZoneType zone_index = 0)
	{
		Instance<DATABASE_DECLARATION> instance;
		instance.m_indirection_index = internal::AllocInstance(DATABASE_DECLARATION::s_database, zone_index, DATABASE_DECLARATION::template EntityTypeIndex<ENTITY_TYPE>());

		return instance;
	}
}

#endif //ENTITY_COMPONENT_SYSTEM_H_
