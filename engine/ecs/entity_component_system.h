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
		//Add component
		template<typename COMPONENT>
		void Add()
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
	};

	//Create database from a database description with the component lists
	Database* CreateDatabase(const DatabaseDesc& database_desc);
	void DestroyDatabase(Database* database);
}

#endif //ENTITY_COMPONENT_SYSTEM_H_
