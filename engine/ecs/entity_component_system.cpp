#include "entity_component_system.h"
#include <core/virtual_buffer.h>
#include <core/sync.h>
#include <memory>
#include <core/profile.h>
#include <job/job_helper.h>

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

	struct InstanceCount
	{
		//Current instances in the world
		size_t count = 0;
		//Current instances in the world plus created
		//Just created instances can not be access in the current frame, just after a tick in the database
		size_t count_created = 0;
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
		std::unique_ptr< std::unique_ptr<core::VirtualBuffer>[]> m_component_containers;

		//Flat list of spin locks to control access to the components
		//Dimensions are <Zone, EntityType>
		std::unique_ptr<core::Mutex[]> m_components_spinlock_mutex;

		//Flat list with the number of instances for each Zone/EntityType
		//Dimensions are <Zone, EntityType>
		std::vector<InstanceCount> m_num_instances;

		struct IndirectionInstanceTable
		{
			//Using virtual memory here will avoid the sync needed in case the table gets reallocated
			core::VirtualBufferTypedInitied<InternalInstanceIndex, 20 * 1024 * 1024> table;
			InstanceIndexType first_free_slot_indirection_instance; //We use the same type that the instance index, as we create the chain with it
		};

		//List of indirection instance indexes and Index to the first free slot avaliable in the indirection instance table for each job
		job::ThreadData<IndirectionInstanceTable> m_indirection_instance_table;

		//List of deferred instance deletes
		job::ThreadData<std::vector<InstanceIndirectionIndexType>> m_deferred_instance_deletes;

		//List of deferred instance moves
		job::ThreadData <std::vector<InstanceMove>> m_deferred_instance_moves;

		//Looked, used for detecting bad access patters
		bool m_locked = false;

		//Stats from last frames
		DatabaseStats m_stats;

		//Callback function
		std::function<void (DababaseTransaction, ZoneType, EntityTypeType, InstanceIndexType, ZoneType, EntityTypeType, InstanceIndexType)> m_callback_function;

		//Get indirection index
		InternalInstanceIndex& AccessInternalInstanceIndex(const InstanceIndirectionIndexType& indirection_index)
		{
			return m_indirection_instance_table.AccessThreadData(indirection_index.thread_id).table[indirection_index.index];
		}

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

			return *m_component_containers[index];
		}

		//Access to component data
		void* GetComponentData(const InternalInstanceIndex& internal_instance_index, ComponentType component_index)
		{
			uint8_t* data = reinterpret_cast<uint8_t*>(GetStorage(internal_instance_index.zone_index, internal_instance_index.entity_type_index, component_index).GetPtr());
			assert(data);

			//Offset to the correct instance component
			return data + internal_instance_index.instance_index * m_components[component_index].size;
		}

		//Can be called outside the tick database
		InstanceIndexType GetNumInstances(ZoneType zone_index, EntityTypeType entity_type_index) const
		{
			const size_t index = entity_type_index + zone_index * m_num_entity_types;

			return static_cast<InstanceIndexType>(m_num_instances[index].count);
		}

		//Can be called outside the tick database
		InstanceIndexType AddInstanceCount(ZoneType zone_index, EntityTypeType entity_type_index)
		{
			const size_t index = entity_type_index + zone_index * m_num_entity_types;
			return static_cast<InstanceIndexType>(m_num_instances[index].count_created++);
		}

		//Can not be called outside the tick of the database
		InstanceIndexType RemoveInstanceCount(ZoneType zone_index, EntityTypeType entity_type_index)
		{
			assert(m_locked);
			const size_t index = entity_type_index + zone_index * m_num_entity_types;
			return static_cast<InstanceIndexType>(--m_num_instances[index].count_created);
		}

		InstanceIndexType AllocInstance(ZoneType zone_index, EntityTypeType entity_type_index)
		{
			core::MutexGuard component_access(m_components_spinlock_mutex[entity_type_index + zone_index * m_num_entity_types]);

			const InstanceIndexType instance_index = AddInstanceCount(zone_index, entity_type_index);

			//Grow all components
			const size_t component_begin_index = GetBeginContainerIndex(zone_index, entity_type_index);
			for (size_t i = 0; i < m_num_components; ++i)
			{
				auto& component_container = m_component_containers[component_begin_index + i];

				if (component_container->GetPtr())
				{
					//Grow the size, instance_index is the index of the last instance in the container
					component_container->SetCommitedSize((instance_index + 1) * m_components[i].size);
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
			assert(m_locked);

			const InstanceIndexType last_instance_index = RemoveInstanceCount(internal_instance_index.zone_index, internal_instance_index.entity_type_index);

			//Check if the instance to delete is the last instance, then is nothing to do
			const bool needs_to_move = last_instance_index != internal_instance_index.instance_index;

			if (m_callback_function)
			{
				m_callback_function(DababaseTransaction::Deletion, internal_instance_index.zone_index, internal_instance_index.entity_type_index, internal_instance_index.instance_index, 0, 0, 0);
				if (needs_to_move)
				{
					m_callback_function(DababaseTransaction::Move, internal_instance_index.zone_index, internal_instance_index.entity_type_index, internal_instance_index.instance_index,
						internal_instance_index.zone_index, internal_instance_index.entity_type_index, last_instance_index);
				}
			}

			//Reduce by one all components
			const size_t component_begin_index = GetBeginContainerIndex(internal_instance_index.zone_index, internal_instance_index.entity_type_index);
			for (size_t i = 0; i < m_num_components; ++i)
			{
				auto& component_container = m_component_containers[component_begin_index + i];
				const size_t component_size = m_components[i].size;
				if (component_container->GetPtr())
				{
					uint8_t* last_instance_data = reinterpret_cast<uint8_t*>(component_container->GetPtr()) + last_instance_index * component_size;
					uint8_t* to_delete_instance_data = reinterpret_cast<uint8_t*>(component_container->GetPtr()) + internal_instance_index.instance_index * component_size;

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
					component_container->SetCommitedSize(last_instance_index * component_size);

					if (needs_to_move && (i == m_indirection_index_component_index))
					{
						//The internal index of the indirection index table needs to be fixup
						//In this moment the to_delete_instance_data has the correct index moved
						auto& indirection_index_component = *(reinterpret_cast<InstanceIndirectionIndexType*>(to_delete_instance_data));
						auto& destroy_internal_instance_index = AccessInternalInstanceIndex(indirection_index_component);

						assert(destroy_internal_instance_index.instance_index == last_instance_index);

						destroy_internal_instance_index = internal_instance_index;
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
			assert(m_locked);

			//Get a copy, as it is going to be change after the move
			InternalInstanceIndex old_internal_instance_index = internal_instance_index;

			//Allocate new instance in the new zone
			const InternalInstanceIndex new_zone_internal_instance_index{ new_zone_index, internal_instance_index.entity_type_index, AllocInstance(new_zone_index, internal_instance_index.entity_type_index) };

			//Move components to the new zone
			const size_t component_begin_index_old_zone = GetBeginContainerIndex(internal_instance_index.zone_index, internal_instance_index.entity_type_index);
			const size_t component_begin_index_new_zone = GetBeginContainerIndex(new_zone_internal_instance_index.zone_index, new_zone_internal_instance_index.entity_type_index);
			for (size_t i = 0; i < m_num_components; ++i)
			{
				auto& old_zonecomponent_container = m_component_containers[component_begin_index_old_zone + i];
				const size_t component_size = m_components[i].size;
				if (old_zonecomponent_container->GetPtr())
				{
					auto& new_zonecomponent_container = m_component_containers[component_begin_index_new_zone + i];
					uint8_t* old_zone_component_instance_data = reinterpret_cast<uint8_t*>(old_zonecomponent_container->GetPtr()) + internal_instance_index.instance_index * component_size;
					uint8_t* new_zone_component_instance_data = reinterpret_cast<uint8_t*>(new_zonecomponent_container->GetPtr()) + new_zone_internal_instance_index.instance_index * component_size;

					
					//Move components (the indirection will move as well, as the last component is the indirection index
					m_components[i].move_operator(new_zone_component_instance_data, old_zone_component_instance_data);
					
					if (i == m_indirection_index_component_index)
					{
						//The internal index of the indirection index table needs to be fixup
						auto& indirection_index_component = *(reinterpret_cast<InstanceIndirectionIndexType*>(old_zone_component_instance_data));
						
						auto& new_internal_instance_index = AccessInternalInstanceIndex(indirection_index_component);

						assert(new_internal_instance_index.zone_index == internal_instance_index.zone_index);
						assert(new_internal_instance_index.entity_type_index == internal_instance_index.entity_type_index);
						assert(new_internal_instance_index.instance_index == internal_instance_index.instance_index);

						new_internal_instance_index = new_zone_internal_instance_index;

						assert(new_internal_instance_index.zone_index == new_zone_index);
					}
				}
			}

			if (m_callback_function)
			{
				m_callback_function(DababaseTransaction::Move, new_zone_internal_instance_index.zone_index, new_zone_internal_instance_index.entity_type_index, new_zone_internal_instance_index.instance_index,
					old_internal_instance_index.zone_index, old_internal_instance_index.entity_type_index, old_internal_instance_index.instance_index);
			}


			//Call Destroy old instance but without destructor
			DestroyInstance(old_internal_instance_index, false);
		}

		InstanceIndirectionIndexType AllocIndirectionIndex(const InternalInstanceIndex& internal_instance_index)
		{
			//Allocate the index in the thread data table
			auto& first_free_slot_indirection_instance_table = m_indirection_instance_table.Get().first_free_slot_indirection_instance;
			auto& indirection_instance_table = m_indirection_instance_table.Get().table;

			if (first_free_slot_indirection_instance_table == -1)
			{
				//The pool is full, just push and index and return
				assert(indirection_instance_table.GetSize() <= (1 << 24));
				indirection_instance_table.PushBack(internal_instance_index);

				return InstanceIndirectionIndexType{ static_cast<uint32_t>(job::GetWorkerIndex()), static_cast<uint32_t>((indirection_instance_table.GetSize() - 1)) };
			}
			else
			{
				//Use the free slot
				assert(indirection_instance_table[first_free_slot_indirection_instance_table].zone_index == InternalInstanceIndex::kFreeSlot);

				InstanceIndexType next_free_slot = indirection_instance_table[first_free_slot_indirection_instance_table].instance_index;
				auto allocated_indirection_index = first_free_slot_indirection_instance_table;
				indirection_instance_table[first_free_slot_indirection_instance_table] = internal_instance_index;
				first_free_slot_indirection_instance_table = next_free_slot;

				return InstanceIndirectionIndexType{ static_cast<uint32_t>(job::GetWorkerIndex()), static_cast<uint32_t>(allocated_indirection_index) }; ;
			}
		}

		void DeallocIndirectionIndex(const InstanceIndirectionIndexType& indirection_index)
		{
			assert(m_locked);

			//Deallocate the index in the thread data table
			auto& first_free_slot_indirection_instance_table = m_indirection_instance_table.AccessThreadData(indirection_index.thread_id).first_free_slot_indirection_instance;
			auto& indirection_instance_table = m_indirection_instance_table.AccessThreadData(indirection_index.thread_id).table;

			//Mark as free and redirect the first free slot to it and update the chain
			auto& internal_instance_index = indirection_instance_table[indirection_index.index];
			internal_instance_index.zone_index = InternalInstanceIndex::kFreeSlot;
			internal_instance_index.instance_index = first_free_slot_indirection_instance_table;

			first_free_slot_indirection_instance_table = indirection_index.index;
		}

		InstanceIndirectionIndexType& GetIndirectionIndex(ZoneType zone_index, EntityTypeType entity_type_index, InstanceIndexType instance_index)
		{
			InternalInstanceIndex internal_index;
			internal_index.zone_index = zone_index;
			internal_index.entity_type_index = entity_type_index;
			internal_index.instance_index = instance_index;

			//Get indirection index
			auto& indirection_index = *reinterpret_cast<InstanceIndirectionIndexType*>(GetComponentData(internal_index, m_indirection_index_component_index));

			auto& internal_instance_index = AccessInternalInstanceIndex(indirection_index);

			assert(internal_instance_index.zone_index == internal_index.zone_index);
			assert(internal_instance_index.entity_type_index == internal_index.entity_type_index);
			assert(internal_instance_index.instance_index == internal_index.instance_index);

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
			database->m_component_containers = std::make_unique<std::unique_ptr<core::VirtualBuffer>[]>(num_components);

			size_t component_array_index = 0;
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
						database->m_component_containers[component_array_index++] = std::make_unique<core::VirtualBuffer>((((1ULL << component_index) & entity_type_mask) != 0 ) ? compoment_buffer_size : 0);
					}
				}
			}
			
			//Init all num of instances to zero, default constructor
			database->m_num_instances.resize(database->m_num_zones * database->m_num_entity_types);
		
			//Init mutex for access to each instance components
			database->m_components_spinlock_mutex = std::make_unique<core::Mutex[]>(database->m_num_zones * database->m_num_entity_types);

			//Reserve memory for the indirection indexes
			database->m_indirection_instance_table.Visit([](auto& data)
				{
					data.first_free_slot_indirection_instance = -1;
				});

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
			assert(!database->m_locked);

			//Add to the deferred list
			database->m_deferred_instance_deletes.Get().push_back(indirection_index);

		}

		void DeallocInstance(Database * database, ZoneType zone_index, EntityTypeType entity_type, InstanceIndexType instance_index)
		{
			//Dealloc it
			DeallocInstance(database, database->GetIndirectionIndex(zone_index, entity_type, instance_index));
		}

		void MoveZoneInstance(Database * database, InstanceIndirectionIndexType index, ZoneType new_zone_index)
		{
			assert(!database->m_locked);

			auto& internal_index = database->AccessInternalInstanceIndex(index);
			if (internal_index.zone_index != new_zone_index)
			{
				//Add to the deferred moves
				database->m_deferred_instance_moves.Get().push_back({ index , new_zone_index });
			}
		}

		void MoveZoneInstance(Database * database, ZoneType zone_index, EntityTypeType entity_type, InstanceIndexType instance_index, ZoneType new_zone_index)
		{
			MoveZoneInstance(database, database->GetIndirectionIndex(zone_index, entity_type, instance_index), new_zone_index);
		}

		void* GetComponentData(Database * database, InstanceIndirectionIndexType indirection_index, ComponentType component_index)
		{
			auto& internal_index = database->AccessInternalInstanceIndex(indirection_index);

			return database->GetComponentData(internal_index, component_index);
		}

		bool InstanceCompare(Database* database, InstanceIndirectionIndexType a_index, ZoneType b_zone, EntityTypeType b_entity_type, InstanceIndexType b_instance_index)
		{
			auto& internal_index = database->AccessInternalInstanceIndex(a_index);
			return internal_index.zone_index == b_zone && internal_index.entity_type_index == b_entity_type && internal_index.instance_index == b_instance_index;
		}

		size_t GetInstanceType(Database * database, InstanceIndirectionIndexType indirection_index)
		{
			auto instance = database->AccessInternalInstanceIndex(indirection_index);
			return instance.entity_type_index;
		}

		EntityTypeMask GetInstanceTypeMask(Database * database, InstanceIndirectionIndexType indirection_index)
		{
			auto instance = database->AccessInternalInstanceIndex(indirection_index);
			return database->m_entity_types[instance.entity_type_index];
		}

		EntityTypeMask GetInstanceTypeMask(Database * database, EntityTypeType entity_type)
		{
			return database->m_entity_types[entity_type];
		}

		size_t GetInstanceTypeIndex(Database* database, InstanceIndirectionIndexType index)
		{
			return database->AccessInternalInstanceIndex(index).entity_type_index;
		}

		ZoneType GetInstanceZone(Database* database, InstanceIndirectionIndexType index)
		{
			return database->AccessInternalInstanceIndex(index).zone_index;
		}

		void TickDatabase(Database* database)
		{
			//Lock database
			database->m_locked = true;

			//Process deletes
			database->m_stats.num_deferred_deletions = 0;
			database->m_deferred_instance_deletes.Visit([&](std::vector<InstanceIndirectionIndexType>& deferred_instance_deletes)
				{
					for (auto& deferred_deleted_indirection_index : deferred_instance_deletes)
					{
						//Get internal instance index and check if it is already deleted
						auto& internal_instance_index = database->AccessInternalInstanceIndex(deferred_deleted_indirection_index);
						if (internal_instance_index.zone_index != InternalInstanceIndex::kFreeSlot)
						{
							//Destroy components associated to this instance
							database->DestroyInstance(internal_instance_index);

							//Deallocate indirection index
							database->DeallocIndirectionIndex(deferred_deleted_indirection_index);
						}
					}
					database->m_stats.num_deferred_deletions += deferred_instance_deletes.size();
					deferred_instance_deletes.clear();
				});

			//Process moves
			database->m_stats.num_deferred_moves = 0;
			database->m_deferred_instance_moves.Visit([&](std::vector<InstanceMove>& deferred_instance_moves)
				{
					for (auto& deferred_move_instance : deferred_instance_moves)
					{
						//Get internal instance index and check if it is already deleted
						auto& internal_instance_index = database->AccessInternalInstanceIndex(deferred_move_instance.indirection_index);
						if (internal_instance_index.zone_index != InternalInstanceIndex::kFreeSlot)
						{
							//Move components associated to the instance
							if (internal_instance_index.zone_index != deferred_move_instance.new_zone)
							{
								database->MoveInstance(internal_instance_index, deferred_move_instance.new_zone);
							}

							//Check that the zone is the correct one
							assert(internal_instance_index.zone_index == deferred_move_instance.new_zone);
						}
					}
					database->m_stats.num_deferred_moves = deferred_instance_moves.size();
					deferred_instance_moves.clear();
				});

	
			//Moves and deleted are using the count_created as well as entities created during the last frame, update count 
			for (auto& instance_count : database->m_num_instances)
			{
				instance_count.count = instance_count.count_created;
			}

			//Unlock database
			database->m_locked = false;
		}
		void SetCallbackTransaction(Database* database, CallbackInternalFunction&& callback)
		{
			database->m_callback_function = callback;
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

		void GetDatabaseStats(Database * database, DatabaseStats & stats)
		{
			assert(!database->m_locked);
			stats = database->m_stats;
		}
		InstanceReference GetInstanceReference(Database* database, ZoneType zone_index, EntityTypeType entity_type, ComponentType component_index)
		{
			return InstanceReference(database->GetIndirectionIndex(zone_index, entity_type, component_index));
		}
	}
}