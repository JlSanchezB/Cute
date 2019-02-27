#include "entity_component_system.h"
#include <core/virtual_buffer.h>

namespace ecs
{
	//Represent a instance (in one zone, one instance type and the index)
	//It is used for handling the pool of instances index, so if zone_index is -1 means it is free
	//and instance_index represent the next free slot
	struct InternalInstanceIndex
	{
		constexpr static ZoneType kFreeSlot = 0xFFFF;
		ZoneType zone_index;
		EntityTypeType entity_type_index;
		InstanceIndexType instance_index;
	};

	struct InstanceMove
	{
		InstanceIndirectionIndexType indirection_index;
		ZoneType new_zone;
	};

	//Database
	//Our Database will have a list of instance types
	//Instance types depends of the combination of components added to a instance
	//Each instance type will have a size and a set of memory blocks with all compoments memory
	struct Database
	{
		//Fast access data
		ComponentType m_num_components;
		EntityTypeType m_num_entity_types;
		ZoneType m_num_zones;

		//Index of the component with the indirection index
		uint8_t m_indirection_index_component_index;

		//List of components
		std::vector<Component> m_components;

		//List of entity types
		std::vector<EntityTypeMask> m_entity_types;

		//Flat list of all component containers (one virtual buffer for each)
		//Dimensions are <Zone, EntityType, Component>
		std::vector<core::VirtualBuffer> m_component_containers;

		//Flat list with the number of instances for each Zone/EntityType
		//Dimensions are <Zone, EntityType>
		std::vector<size_t> m_num_instances;

		//List of indirection instance indexes
		std::vector<InternalInstanceIndex> m_indirection_instance_table;

		//Index to the first free slot avaliable in the indirection instance table
		InstanceIndexType m_first_free_slot_indirection_instance_table = -1;

		//List of deferred instance deletes
		std::vector<InstanceIndexType> m_deferred_instance_deletes;

		//List of deferred instance moves
		std::vector< InstanceMove> m_deferred_instance_moves;

		//Get container index
		size_t GetContainerIndex(ZoneType zone_index, EntityTypeType entity_type_index, ComponentType component_index) const
		{
			return component_index + m_num_components * entity_type_index + zone_index * (m_num_components * m_num_entity_types);
		}

		//Get begin container index for all components
		size_t GetBeginContainerIndex(ZoneType zone_index, EntityTypeType entity_type_index) const
		{
			return m_num_components * entity_type_index + zone_index * (m_num_components * m_num_entity_types);
		}

		//Access of the component virtual buffer
		core::VirtualBuffer& GetStorage(ZoneType zone_index, EntityTypeType entity_type_index, ComponentType component_index)
		{
			const size_t index = GetContainerIndex(zone_index, entity_type_index, component_index);

			return m_component_containers[index];
		}

		//Access to component data
		void* GetComponentData(const InternalInstanceIndex& internal_instance_index, ComponentType component_index)
		{
			uint8_t* data = reinterpret_cast<uint8_t*>(GetStorage(internal_instance_index.zone_index, internal_instance_index.entity_type_index, component_index).GetPtr());
			assert(data);

			//Offset to the correct instance component
			return data + internal_instance_index.instance_index * m_components[component_index].size;
		}

		InstanceIndexType GetNumInstances(ZoneType zone_index, EntityTypeType entity_type_index) const
		{
			const size_t index = entity_type_index + zone_index * m_num_entity_types;

			return static_cast<InstanceIndexType>(m_num_instances[index]);
		}

		InstanceIndexType AddInstanceCount(ZoneType zone_index, EntityTypeType entity_type_index)
		{
			const size_t index = entity_type_index + zone_index * m_num_entity_types;
			return static_cast<InstanceIndexType>(m_num_instances[index]++);
		}

		InstanceIndexType RemoveInstanceCount(ZoneType zone_index, EntityTypeType entity_type_index)
		{
			const size_t index = entity_type_index + zone_index * m_num_entity_types;
			return static_cast<InstanceIndexType>(--m_num_instances[index]);
		}

		InstanceIndexType AllocInstance(ZoneType zone_index, EntityTypeType entity_type_index)
		{
			const InstanceIndexType instance_index = AddInstanceCount(zone_index, entity_type_index);

			//Grow all components
			const size_t component_begin_index = GetBeginContainerIndex(zone_index, entity_type_index);
			for (size_t i = 0; i < m_num_components; ++i)
			{
				auto& component_container = m_component_containers[component_begin_index + i];

				if (component_container.GetPtr())
				{
					//Grow the size
					component_container.SetCommitedSize(GetNumInstances(zone_index, entity_type_index) * m_components[i].size);
				}
			}

			return instance_index;
		}

		//Destroy instance
		//Destroy components associated to this instance
		//Move last instance to the gap left by the deleted instance
		//Fix all the redirections
		void DestroyInstance(const InternalInstanceIndex& internal_instance_index, bool needs_destructor_call = true)
		{
			const InstanceIndexType last_instance_index = RemoveInstanceCount(internal_instance_index.zone_index, internal_instance_index.entity_type_index);

			//Check if the instance to delete is the last instance, then is nothing to do
			const bool needs_to_move = last_instance_index != internal_instance_index.instance_index;

			//Reduce by one all components
			const size_t component_begin_index = GetBeginContainerIndex(internal_instance_index.zone_index, internal_instance_index.entity_type_index);
			for (size_t i = 0; i < m_num_components; ++i)
			{
				auto& component_container = m_component_containers[component_begin_index + i];
				const size_t component_size = m_components[i].size;
				if (component_container.GetPtr())
				{
					uint8_t* last_instance_data = reinterpret_cast<uint8_t*>(component_container.GetPtr()) + last_instance_index * component_size;
					uint8_t* to_delete_instance_data = reinterpret_cast<uint8_t*>(component_container.GetPtr()) + internal_instance_index.instance_index * component_size;

					if (needs_destructor_call)
					{
						//Call the destructor for this component
						m_components[i].destructor_operator(to_delete_instance_data);
					}

					if (needs_to_move)
					{
						//Move components (the indirection will move as well, as the last component is the indirection index
						m_components[i].move_operator(to_delete_instance_data, last_instance_data);
					}

					//Reduce the size of the component storage
					component_container.SetCommitedSize(last_instance_index * component_size);

					if (i == m_indirection_index_component_index)
					{
						//The internal index of the indirection index table needs to be fixup
						//In this moment the to_delete_instance_data has the correct index moved
						auto& indirection_index_component = *(reinterpret_cast<InstanceIndirectionIndexType*>(to_delete_instance_data));
						m_indirection_instance_table[indirection_index_component] = internal_instance_index;
					}
				}
			}
		}

		//Move Instance
		//Allocate a new instance in the new zone
		//Move the components from the old zone to the new one
		//Fix indirection index
		//Move last instance from the old zone to the new gap
		//Reduce old zone size
		//Fix indirection index
		void MoveInstance(const InternalInstanceIndex& internal_instance_index, ZoneType new_zone_index)
		{
			//Allocate new instance in the new zone
			const InternalInstanceIndex new_zone_internal_instance_index{ new_zone_index, internal_instance_index.entity_type_index, AllocInstance(new_zone_index, internal_instance_index.entity_type_index) };

			//Move components to the new zone
			const size_t component_begin_index_old_zone = GetBeginContainerIndex(internal_instance_index.zone_index, internal_instance_index.entity_type_index);
			const size_t component_begin_index_new_zone = GetBeginContainerIndex(new_zone_internal_instance_index.zone_index, new_zone_internal_instance_index.entity_type_index);
			for (size_t i = 0; i < m_num_components; ++i)
			{
				auto& old_zonecomponent_container = m_component_containers[component_begin_index_old_zone + i];
				const size_t component_size = m_components[i].size;
				if (old_zonecomponent_container.GetPtr())
				{
					uint8_t* old_zone_component_instance_data = reinterpret_cast<uint8_t*>(old_zonecomponent_container.GetPtr()) + internal_instance_index.instance_index * component_size;
					uint8_t* new_zone_component_instance_data = reinterpret_cast<uint8_t*>(old_zonecomponent_container.GetPtr()) + new_zone_internal_instance_index.instance_index * component_size;

					
					//Move components (the indirection will move as well, as the last component is the indirection index
					m_components[i].move_operator(new_zone_component_instance_data, old_zone_component_instance_data);
					
					if (i == m_indirection_index_component_index)
					{
						//The internal index of the indirection index table needs to be fixup
						auto& indirection_index_component = *(reinterpret_cast<InstanceIndirectionIndexType*>(old_zone_component_instance_data));
						m_indirection_instance_table[indirection_index_component] = new_zone_internal_instance_index;
					}
				}
			}

			//Call Destroy old instance but without destructor
			DestroyInstance(internal_instance_index, false);
		}

		InstanceIndirectionIndexType AllocIndirectionIndex(const InternalInstanceIndex& internal_instance_index)
		{
			if (m_first_free_slot_indirection_instance_table == -1)
			{
				//The pool is full, just push and index and return
				m_indirection_instance_table.push_back(internal_instance_index);
				assert(m_indirection_instance_table.size() < std::numeric_limits<InstanceIndirectionIndexType>::max());
				return static_cast<InstanceIndirectionIndexType>(m_indirection_instance_table.size()-1);
			}
			else
			{
				//Use the free slot
				assert(m_indirection_instance_table[m_first_free_slot_indirection_instance_table].zone_index == InternalInstanceIndex::kFreeSlot);
				InstanceIndexType next_free_slot = m_indirection_instance_table[m_first_free_slot_indirection_instance_table].instance_index;
				InstanceIndirectionIndexType allocated_indirection_index = m_first_free_slot_indirection_instance_table;
				m_indirection_instance_table[m_first_free_slot_indirection_instance_table] = internal_instance_index;
				m_first_free_slot_indirection_instance_table = next_free_slot;

				return allocated_indirection_index;
			}
		}

		void DeallocIndirectionIndex(const InstanceIndirectionIndexType& indirection_index)
		{
			//Mark as free and redirect the first free slot to it and update the chain
			auto& internal_instance_index = m_indirection_instance_table[indirection_index];
			internal_instance_index.zone_index = InternalInstanceIndex::kFreeSlot;
			internal_instance_index.instance_index = m_first_free_slot_indirection_instance_table;
			m_first_free_slot_indirection_instance_table = indirection_index;
		}

		InstanceIndirectionIndexType& GetIndirectionIndex(ZoneType zone_index, EntityTypeType entity_type_index, InstanceIndexType instance_index)
		{
			InternalInstanceIndex internal_index;
			internal_index.zone_index = zone_index;
			internal_index.entity_type_index = entity_type_index;
			internal_index.instance_index = instance_index;

			//Get indirection index
			auto& indirection_index = *reinterpret_cast<InstanceIndirectionIndexType*>(GetComponentData(internal_index, m_indirection_index_component_index));

			assert(m_indirection_instance_table[indirection_index].zone_index == internal_index.zone_index);
			assert(m_indirection_instance_table[indirection_index].entity_type_index == internal_index.entity_type_index);
			assert(m_indirection_instance_table[indirection_index].instance_index == internal_index.instance_index);

			return indirection_index;
		}
	};

	namespace internal
	{
		Database* CreateDatabase(const DatabaseDesc & database_desc, const std::vector<Component>& components, const std::vector<EntityTypeMask> entity_types)
		{
			assert(database_desc.num_zones > 0);
			assert(database_desc.num_zones < std::numeric_limits<ZoneType>::max());
			assert(entity_types.size() < std::numeric_limits<EntityTypeType>::max());
			assert(components.size() < 63);

			Database* database = new Database();

			//Init sizes
			database->m_num_zones = static_cast<ZoneType>(database_desc.num_zones);
			database->m_num_components = static_cast<ComponentType>(components.size());
			database->m_num_entity_types = static_cast<EntityTypeType>(entity_types.size());

			//Get all components information
			database->m_components = components;

			//Create extra component with the index of the instance in the indirection table
			database->m_indirection_index_component_index = database->m_num_components;
			database->m_num_components++;
			Component indirection_index_component;
			indirection_index_component.Capture<InstanceIndirectionIndexType>();
			database->m_components.push_back(indirection_index_component);

			//Get all entity types
			database->m_entity_types = entity_types;

			//Add the new indirection component to ALL the entity types
			for (auto& entity_type : database->m_entity_types)
			{
				entity_type |= (1ULL << database->m_indirection_index_component_index);
			}

			//Create all the components
			const size_t num_components = database->m_num_zones * database->m_num_entity_types * database->m_num_components;
			database->m_component_containers.reserve(num_components);

			//For each zone add the entity types
			for (size_t zone_index = 0; zone_index < database->m_num_zones; ++zone_index)
			{
				//Add all entity types registered
				for (size_t entity_type_index = 0; entity_type_index < entity_types.size(); ++entity_type_index)
				{
					auto& entity_type_mask = database->m_entity_types[entity_type_index];

					//Create all virtual buffers for each component
					for (size_t component_index = 0; component_index < database->m_num_components; ++component_index)
					{
						const size_t compoment_buffer_size = database_desc.num_max_entities_zone * database->m_components[component_index].size;
						database->m_component_containers.emplace_back((((1ULL << component_index) & entity_type_mask) != 0 ) ? compoment_buffer_size : 0);
					}
				}
			}
			
			//Init all num of instances to zero
			database->m_num_instances.resize(database->m_num_zones * database->m_num_entity_types);
			for (auto& num_instance : database->m_num_instances)
			{
				num_instance = 0;
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

		InstanceIndirectionIndexType AllocInstance(Database * database, ZoneType zone_index, EntityTypeType entity_type_index)
		{
			//Alloc component data for this instance and create internal_instance_index
			InternalInstanceIndex internal_index { zone_index, entity_type_index, database->AllocInstance(zone_index, entity_type_index)};

			//Allocate indirection index
			InstanceIndirectionIndexType indirection_index = database->AllocIndirectionIndex(internal_index);

			//Set the indirection index into the correct component
			auto& indirection_index_component = *(reinterpret_cast<InstanceIndirectionIndexType*>(database->GetComponentData(internal_index, database->m_indirection_index_component_index)));
			indirection_index_component = indirection_index;

			return indirection_index;
		}
		void DeallocInstance(Database * database, InstanceIndirectionIndexType indirection_index)
		{
			//Add to the deferred list
			database->m_deferred_instance_deletes.push_back(indirection_index);
		}

		void DeallocInstance(Database * database, ZoneType zone_index, EntityTypeType entity_type, InstanceIndexType instance_index)
		{
			//Dealloc it
			DeallocInstance(database, database->GetIndirectionIndex(zone_index, entity_type, instance_index));
		}

		void MoveZoneInstance(Database * database, InstanceIndirectionIndexType index, ZoneType new_zone_index)
		{
			//Add to the deferred moves
			database->m_deferred_instance_moves.emplace_back(InstanceMove{ index , new_zone_index });
		}

		void MoveZoneInstance(Database * database, ZoneType zone_index, EntityTypeType entity_type, InstanceIndexType instance_index, ZoneType new_zone_index)
		{
			MoveZoneInstance(database, database->GetIndirectionIndex(zone_index, entity_type, instance_index), new_zone_index);
		}

		void* GetComponentData(Database * database, InstanceIndirectionIndexType indirection_index, ComponentType component_index)
		{
			auto internal_index = database->m_indirection_instance_table[indirection_index];

			return database->GetComponentData(internal_index, component_index);
		}

		size_t GetInstanceType(Database * database, InstanceIndirectionIndexType indirection_index)
		{
			auto instance = database->m_indirection_instance_table[indirection_index];
			return instance.entity_type_index;
		}

		EntityTypeMask GetInstanceTypeMask(Database * database, InstanceIndirectionIndexType indirection_index)
		{
			auto instance = database->m_indirection_instance_table[indirection_index];
			return database->m_entity_types[instance.entity_type_index];
		}

		EntityTypeMask GetInstanceTypeMask(Database * database, EntityTypeType entity_type)
		{
			return database->m_entity_types[entity_type];
		}

		void TickDatabase(Database* database)
		{
			//Process deletes
			for (auto& deferred_deleted_indirection_index : database->m_deferred_instance_deletes)
			{
				//Get internal instance index and check if it is already deleted
				auto& internal_instance_index = database->m_indirection_instance_table[deferred_deleted_indirection_index];
				if (internal_instance_index.zone_index != InternalInstanceIndex::kFreeSlot)
				{
					//Destroy components associated to this instance
					database->DestroyInstance(internal_instance_index);

					//Deallocate indirection index
					database->DeallocIndirectionIndex(deferred_deleted_indirection_index);
				}
				
			}
			database->m_deferred_instance_deletes.clear();

			//Process moves
			for (auto& deferred_move_instance : database->m_deferred_instance_moves)
			{
				//Get internal instance index and check if it is already deleted
				auto& internal_instance_index = database->m_indirection_instance_table[deferred_move_instance.indirection_index];
				if (internal_instance_index.zone_index != InternalInstanceIndex::kFreeSlot)
				{
					//Move components associated to the instance
					if (internal_instance_index.zone_index != deferred_move_instance.new_zone)
					{
						database->MoveInstance(internal_instance_index, deferred_move_instance.new_zone);
					}
				}
			}
		}
		ZoneType GetNumZones(Database * database)
		{
			return database->m_num_zones;
		}

		void* GetStorageComponent(Database * database, ZoneType zone_index, EntityTypeType entity_type, ComponentType component_index)
		{
			return database->GetStorage(zone_index, entity_type, component_index).GetPtr();
		}

		InstanceIndexType GetNumInstances(Database * database, ZoneType zone_index, EntityTypeType entity_type)
		{
			return database->GetNumInstances(zone_index, entity_type);
		}
	}
}