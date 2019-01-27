#include "entity_component_system.h"
#include <core/virtual_buffer.h>

namespace ecs
{
	using InstanceIndexType = uint32_t;

	//Represent a instance (in one zone, one instance type and the index)
	struct InternalInstanceIndex
	{
		ZoneType zone_index;
		EntityTypeType entity_type_index;
		InstanceIndexType instance_index;
	};

	//Database
	//Our Database will have a list of instance types
	//Instance types depends of the combination of components added to a instance
	//Each instance type will have a size and a set of memory blocks with all compoments memory
	struct Database
	{
		//Fast access data
		size_t m_num_components;
		size_t m_num_entity_types;
		size_t m_num_zones;

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

		//Access of the component virtual buffer
		core::VirtualBuffer& GetStorage(uint16_t zone_index, uint16_t entity_type_index, uint8_t component_index)
		{
			const size_t index = component_index + m_num_components * entity_type_index + zone_index * (m_num_components * m_num_entity_types);

			return m_component_containers[index];
		}

		InstanceIndexType GetNumInstances(uint16_t zone_index, uint16_t entity_type_index) const
		{
			const size_t index = entity_type_index + zone_index * m_num_entity_types;

			return static_cast<InstanceIndexType>(m_num_instances[index]);
		}

		InstanceIndexType AllocInstance(uint16_t zone_index, uint16_t entity_type_index)
		{
			const size_t index = entity_type_index + zone_index * m_num_entity_types;
			const InstanceIndexType instance_index = static_cast<InstanceIndexType>(m_num_instances[index]++);

			//Grow all components
			const size_t component_begin_index = m_num_components * entity_type_index + zone_index * (m_num_components * m_num_entity_types);
			for (size_t i = 0; i < m_num_components; ++i)
			{
				auto& component_container = m_component_containers[component_begin_index + i];

				if (component_container.GetPtr())
				{
					//Grow the size
					component_container.SetCommitedSize((instance_index + 1) * m_components[i].size);
				}
			}

			return instance_index;
		}
	};

	namespace internal
	{
		Database* CreateDatabase(const DatabaseDesc & database_desc, const std::vector<Component>& components, const std::vector<EntityTypeMask> entity_types)
		{
			assert(database_desc.num_zones > 0);
			assert(database_desc.num_zones < std::numeric_limits<uint16_t>::max());
			assert(entity_types.size() < std::numeric_limits<uint16_t>::max());
			assert(components.size() < 64);

			Database* database = new Database();

			//Init sizes
			database->m_num_zones = database_desc.num_zones;
			database->m_num_components = components.size();
			database->m_num_entity_types = entity_types.size();

			//Get all components information
			database->m_components = components;

			//Get all entity types
			database->m_entity_types = entity_types;

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
			InternalInstanceIndex internal_instance_index { zone_index, entity_type_index, database->AllocInstance(zone_index, entity_type_index)};

			//Allocate indirection index
			database->m_indirection_instance_table.emplace_back(internal_instance_index);
			assert(database->m_indirection_instance_table.size() < std::numeric_limits<InstanceIndirectionIndexType>::max());
			
			return static_cast<InstanceIndirectionIndexType>(database->m_indirection_instance_table.size() - 1);
		}
		void DeallocInstance(Database * database, InstanceIndirectionIndexType index)
		{
			//Add to the deallocate buffer
		}

		void * GetComponentData(Database * database, InstanceIndirectionIndexType index, ComponentType component_index)
		{
			auto instance = database->m_indirection_instance_table[index];

			uint8_t* data = reinterpret_cast<uint8_t*>(database->GetStorage(instance.zone_index, instance.entity_type_index, static_cast<uint8_t>(component_index)).GetPtr());

			//It needs to be a valid pointer, if not we are trying to access a component not register in the entity type
			assert(data);

			//Offset to the correct instance component
			return data + database->m_components[component_index].size;
		}

		size_t GetInstanceType(Database * database, InstanceIndirectionIndexType index)
		{
			auto instance = database->m_indirection_instance_table[index];
			return instance.entity_type_index;
		}

		EntityTypeMask GetInstanceTypeMask(Database * database, InstanceIndirectionIndexType index)
		{
			auto instance = database->m_indirection_instance_table[index];
			return database->m_entity_types[instance.entity_type_index];
		}
	}
}