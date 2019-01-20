//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system interface
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_SYSTEM_H_
#define ENTITY_COMPONENT_SYSTEM_H_

#include <vector>

namespace ecs
{
	struct Database;

	struct ComponentInitParameter
	{
		size_t size;
		size_t align;

		//Move operator
		void(*move_operator)(void*, void*);

		//Capture the properties of the component
		template<typename COMPONENT>
		void Init()
		{
			size = sizeof(COMPONENT);
			align = alignof(COMPONENT);

			move_operator = ComponentDesc<COMPONENT>::Move;
		}
	};

	struct DatabaseDesc
	{
		//List of components
		std::vector<ComponentInitParameter> components;
		//Add component
		template<typename COMPONENT>
		void Add()
		{
			static_assert(std::is_convertible<COMPONENT*, Component*>::value, "Component class has to derived from ecs::Component");
			static_assert(ComponentDesc<COMPONENT>::s_component_index == static_cast<size_t>(-1), "Component class can be used only once and only in one Database");

			ComponentInitParameter component_init_parameter;
			component_init_parameter.Init<COMPONENT>();
			components.push_back(component_init_parameter);

			//Set the index to the component index, so it can be accessed faster
			ComponentDesc<COMPONENT>::s_component_index = components.size() - 1;
		}
	};

	//Create database from a database description with the component lists
	Database* CreateDatabase(const DatabaseDesc& database_desc);
	void DestroyDatabase(Database* database);
}

#endif //ENTITY_COMPONENT_SYSTEM_H_
