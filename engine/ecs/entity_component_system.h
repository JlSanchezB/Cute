//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system interface
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_SYSTEM_H_
#define ENTITY_COMPONENT_SYSTEM_H_

#include <vector>

namespace ecs
{
	struct Database;

	//Base class for all components
	//It will not add data per object, just containts information as the component index in the database
	struct Component
	{
		inline static size_t s_component_index = static_cast<size_t>(-1);
	};

	struct ComponentDesc
	{
		size_t size;
		size_t align;

		//Capture the properties of the component
		template<typename COMPONENT>
		void Init()
		{
			size = sizeof(COMPONENT);
			align = alignof(COMPONENT);
		}
	};
	struct DatabaseDesc
	{
		//List of components
		std::vector<ComponentDesc> components;
		//Add component
		template<typename COMPONENT>
		void Add()
		{
			ComponentDesc component_desc;
			component_desc.Init<COMPONENT>();
			components.push_back(component_desc);

			//Set the index to the component index, so it can be accessed faster
			static_assert(std::is_convertible<COMPONENT*, Component*>::value, "Component class has to derived from ecs::Component");
			static_assert(COMPONENT::s_component_index == static_cast<size_t>(-1), "Component class can be used only in one Database");
			COMPONENT::s_component_index = components.size() - 1;
		}
	};

	//Create database from a database description with the component lists
	Database* CreateDatabase(const DatabaseDesc& database_desc);
	void DestroyDatabase(Database* database);
}

#endif //ENTITY_COMPONENT_SYSTEM_H_
