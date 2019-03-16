//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system interface
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_SYSTEM_H_
#define ENTITY_COMPONENT_SYSTEM_H_

#include <vector>
#include <cassert>
#include <type_traits>
#include <core/type_list.h>
#include "entity_component_common.h"
#include "entity_component_instance.h"

namespace ecs
{
	template<typename ...COMPONENTS>
	using ComponentList = core::TypeList<COMPONENTS...>;

	template<typename ...ENTITY_TYPE>
	using EntityTypeList = core::TypeList<ENTITY_TYPE...>;

	template<typename ...COMPONENTS>
	struct EntityType
	{
		//Return the mask that represent this instance type, mask is a bit set with the components enabled
		template<typename DATABASE_DECLARATION>
		constexpr static EntityTypeMask EntityTypeMask()
		{
			return ((1ULL << DATABASE_DECLARATION::Components::template ElementIndex<COMPONENTS>()) | ...);
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
	struct ComponentOperatorsDeclaration
	{
		static void Move(void* ptr_a, void* ptr_b)
		{
			COMPONENT& a = *(reinterpret_cast<COMPONENT*>(ptr_a));
			COMPONENT& b = *(reinterpret_cast<COMPONENT*>(ptr_b));

			a = std::move(b);
		}

		static void Destructor(void* ptr)
		{
			COMPONENT& a = *(reinterpret_cast<COMPONENT*>(ptr));

			a.~COMPONENT();
		}
	};

	//Represent all information needed for the ECS about the component
	struct Component
	{
		size_t size;
		size_t align;

		//Move operator
		void(*move_operator)(void*, void*);

		//Destructor operator
		void(*destructor_operator)(void*);

		//Capture the properties of the component
		template<typename COMPONENT>
		void Capture()
		{
			size = sizeof(COMPONENT);
			align = alignof(COMPONENT);

			move_operator = ComponentOperatorsDeclaration<COMPONENT>::Move;
			destructor_operator = ComponentOperatorsDeclaration<COMPONENT>::Destructor;
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

		//Dealloc instance
		void DeallocInstance(Database* database, ZoneType zone_index, EntityTypeType entity_type, InstanceIndexType instance_index);

		//Move zone
		void MoveZoneInstance(Database* database, InstanceIndirectionIndexType index, ZoneType new_zone_index);

		//Move zone
		void MoveZoneInstance(Database* database, ZoneType zone_index, EntityTypeType entity_type, InstanceIndexType instance_index, ZoneType new_zone_index);

		//Get component data
		void* GetComponentData(Database* database, InstanceIndirectionIndexType index, ComponentType component_index);

		//Get instance type from a indirection index
		size_t GetInstanceType(Database* database, InstanceIndirectionIndexType index);

		//Get instance type mask from a indirection index
		EntityTypeMask GetInstanceTypeMask(Database* database, InstanceIndirectionIndexType index);

		//Get instance type mask from a entity type index
		EntityTypeMask GetInstanceTypeMask(Database* database, EntityTypeType entity_type);

		//Tick database
		void TickDatabase(Database* database);

		//Get num zones
		ZoneType GetNumZones(Database* database);

		//Get storage component buffer
		void* GetStorageComponent(Database* database, ZoneType zone_index, EntityTypeType entity_type, ComponentType component_index);

		//Get storage component buffer helper
		template<typename DATABASE_DECLARATION, typename COMPONENT>
		COMPONENT* GetStorageComponentHelper(ZoneType zone_index, EntityTypeType entity_type)
		{
			return reinterpret_cast<COMPONENT*>(GetStorageComponent(DATABASE_DECLARATION::s_database,
				zone_index,
				entity_type,
				DATABASE_DECLARATION::template ComponentIndex<std::remove_const<COMPONENT>::type>()));
		}

		//Get num instances
		InstanceIndexType GetNumInstances(Database* database, ZoneType zone_index, EntityTypeType entity_type);

		//Get database stats
		void GetDatabaseStats(Database* database, DatabaseStats& stats);
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

	//Dealloc instance
	template<typename DATABASE_DECLARATION>
	void DeallocInstance(Instance<DATABASE_DECLARATION>& instance)
	{
		internal::DeallocInstance(DATABASE_DECLARATION::s_database, instance.m_indirection_index);
		instance.m_indirection_index = -1;
	}

	//Move instance
	template<typename DATABASE_DECLARATION>
	void MoveInstance(Instance<DATABASE_DECLARATION>& instance, ZoneType new_zone_index)
	{
		internal::MoveZoneInstance(DATABASE_DECLARATION::s_database, instance.m_indirection_index, new_zone_index);
	}

	template<typename DATABASE_DECLARATION, typename ENTITY_TYPE>
	size_t GetNumInstances()
	{
		size_t size_count = 0;
		const ZoneType num_zones = internal::GetNumZones(DATABASE_DECLARATION::s_database);
		for (ZoneType zone_index = 0; zone_index < num_zones; ++zone_index)
		{
			size_count += internal::GetNumInstances(DATABASE_DECLARATION::s_database, zone_index, DATABASE_DECLARATION::template EntityTypeIndex<ENTITY_TYPE>());
		}
		return size_count;
	}

	template<typename DATABASE_DECLARATION>
	void GetDatabaseStats(DatabaseStats& stats)
	{
		return internal::GetDatabaseStats(DATABASE_DECLARATION::s_database, stats);
	}

	//Tick database
	//Process all the database deferred tasks as
	//Deallocs and Moves, destructors of the components are called here
	template<typename DATABASE_DECLARATION>
	void Tick()
	{
		internal::TickDatabase(DATABASE_DECLARATION::s_database);
	}

	//Iterator class, helper for access to data of the instance during the process calls
	template<typename DATABASE_DECLARATION>
	class InstanceIterator
	{
	public:
		ZoneType m_zone_index;
		EntityTypeType m_entity_type;
		InstanceIndexType m_instance_index;

		template<typename COMPONENT>
		COMPONENT& Get() const
		{
			return *(reinterpret_cast<COMPONENT*>(internal::GetStorageComponent(DATABASE_DECLARATION::s_database,
				m_zone_index, m_entity_type, DATABASE_DECLARATION::template ComponentIndex<COMPONENT>())) + m_instance_index);
		}

		template<typename COMPONENT>
		bool Contain() const
		{
			return (DATABASE_DECLARATION::template ComponentMask<COMPONENT>() & internal::GetInstanceTypeMask(DATABASE_DECLARATION::s_database, m_entity_type)) != 0;
		}

		void Dealloc() const
		{
			internal::DeallocInstance(DATABASE_DECLARATION::s_database, m_zone_index, m_entity_type, m_instance_index);
		}

		void Move(ZoneType new_zone_index) const
		{
			if (new_zone_index != m_zone_index)
			{
				internal::MoveZoneInstance(DATABASE_DECLARATION::s_database, m_zone_index, m_entity_type, m_instance_index, new_zone_index);
			}
		}

		bool operator==(const InstanceIterator<DATABASE_DECLARATION>& b) const
		{
			return (m_zone_index == b.m_zone_index && m_entity_type == b.m_entity_type && m_instance_index == b.m_instance_index);
		}

		bool operator!=(const InstanceIterator<DATABASE_DECLARATION>& b) const
		{
			return (m_zone_index != b.m_zone_index || m_entity_type != b.m_entity_type || m_instance_index != b.m_instance_index);
		}
	};

	namespace internal
	{
		//Caller helper
		template<typename DATABASE_DECLARATION, size_t ...indices, typename ...Args, typename FUNCTION>
		constexpr inline void caller_helper(FUNCTION&& function, const InstanceIterator<DATABASE_DECLARATION>& instance_it, InstanceIndexType instance_index, std::integer_sequence<size_t, indices...>, std::tuple<Args...> &arguments)
		{
			function(instance_it, std::get<indices>(arguments)[instance_index]...);
		}
	}

	//Process components
	//Kernel function recives an instance and a list of components
	template<typename DATABASE_DECLARATION, typename ...COMPONENTS, typename FUNCTION, typename BITSET>
	void Process(FUNCTION&& kernel, BITSET&& zone_bitset)
	{
		//Calculate component mask
		const EntityTypeMask component_mask = EntityType<std::remove_const<COMPONENTS>::type...>::template EntityTypeMask<DATABASE_DECLARATION>();
		
		const ZoneType num_zones = internal::GetNumZones(DATABASE_DECLARATION::s_database);

		InstanceIterator<DATABASE_DECLARATION> instance_iterator;

		//Loop for all entity type that match the component mask
		core::visit<DATABASE_DECLARATION::EntityTypes::template Size()>([&](auto entity_type_index)
		{
			const EntityTypeType entity_type = static_cast<EntityTypeType>(entity_type_index.value);

			instance_iterator.m_entity_type = entity_type;

			using EntityTypeIt = typename DATABASE_DECLARATION::EntityTypes::template ElementType<entity_type_index.value>;
			if ((component_mask & EntityTypeIt::template EntityTypeMask<DATABASE_DECLARATION>()) == component_mask)
			{
				//Loop all zones in the bitmask
				for (ZoneType zone_index = 0; zone_index < num_zones; ++zone_index)
				{
					if (zone_bitset.test(zone_index))
					{
						instance_iterator.m_zone_index = zone_index;

						const InstanceIndexType num_instances = internal::GetNumInstances(DATABASE_DECLARATION::s_database, zone_index, entity_type);
						auto argument_component_buffers = std::make_tuple(
							internal::GetStorageComponentHelper<DATABASE_DECLARATION, COMPONENTS>(zone_index, entity_type)...);

						//Go for all the instances and call the kernel function
						for (InstanceIndexType instance_index = 0; instance_index < num_instances; ++instance_index)
						{
							instance_iterator.m_instance_index = instance_index;
							
							//Call kernel
							internal::caller_helper<DATABASE_DECLARATION>(kernel, instance_iterator, instance_index, std::make_index_sequence<sizeof...(COMPONENTS)>(), argument_component_buffers);
						}
					}
				}

			}
		});
	}
}

#endif //ENTITY_COMPONENT_SYSTEM_H_
