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
		inline static size_t s_component_index = static_cast<size_t>(-1);

		static void Move(void* ptr_a, void* ptr_b)
		{
			COMPONENT& a = *ptr_a;
			COMPONENT& b = *ptr_b;
			a = std::move(b);
		}
	};
}

//A specialization of the ComponentDesc needs to be defined
#define DECLARE_COMPONENT(COMPONENT) using Component##COMPONENT = ComponentDesc<COMPONENT>;

#endif //ENTITY_COMPONENT_DESC_H_
