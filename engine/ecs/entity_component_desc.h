//////////////////////////////////////////////////////////////////////////
// Cute engine - Descriptor needed for defining a component for the entity component system
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_DESC_H_
#define ENTITY_COMPONENT_DESC_H_

namespace ecs
{
	//Each component needs to be defined in the template struct ComponentDesc
	template<typename COMPONENT>
	struct ComponentDesc
	{
		static void Move(void* ptr_a, void* ptr_b)
		{
			COMPONENT& a = *(reinterpret_cast<COMPONENT*>(ptr_a));
			COMPONENT& b = *(reinterpret_cast<COMPONENT*>(ptr_b));
			
			a = std::move(b);
		}

		using Type = COMPONENT;
	};
}

#endif //ENTITY_COMPONENT_DESC_H_
