//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component instance
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_INSTANCE_H_
#define ENTITY_COMPONENT_INSTANCE_H_

namespace ecs
{
	using InstanceIndirectionIndexType = uint32_t;

	//Templated instance class, needs to be specialized in the client
	template<typename DATABASE_DECLARATION>
	class Instance
	{
	public:
		//Init all the components with the default constructor
		constexpr Instance<DATABASE_DECLARATION> InitDefault();

		//Init Components with constructor
		template<typename ...COMPONENTS, typename ...ARGS>
		constexpr Instance<DATABASE_DECLARATION>& Init(ARGS&&... args);

		//Countains component
		template<typename COMPONENT>
		constexpr bool Contains();

		//Accessor to a component
		template<typename COMPONENT>
		constexpr COMPONENT& Get();

	private:
		InstanceIndirectionIndexType m_indirection_index;

		template<typename DATABASE_DECLARATION, typename ENTITY_TYPE>
		friend Instance<DATABASE_DECLARATION> AllocInstance();
	};

	template<typename DATABASE_DECLARATION>
	constexpr inline Instance<DATABASE_DECLARATION> Instance<DATABASE_DECLARATION>::InitDefault()
	{
		//Calls the default contructor for all components of the instance
		EntityTypeMask entity_type_mask = internal::GetInstanceTypeMask(DATABASE_DECLARATION::s_database, m_indirection_index);

		core::visit<DATABASE_DECLARATION::Components::template Size()>([&](auto component_index)
		{
			if (((1ULL << component_index.value) & entity_type_mask) != 0)
			{
				//Placement new with default constructor
				void* data = internal::GetComponentData(DATABASE_DECLARATION::s_database, m_indirection_index, component_index.value);

				new (data) typename DATABASE_DECLARATION::Components::template ElementType<component_index.value>();
			}
		});

		return *this;
	}
	template<typename DATABASE_DECLARATION>
	template<typename ...COMPONENTS, typename ...ARGS>
	constexpr inline Instance<DATABASE_DECLARATION>& Instance<DATABASE_DECLARATION>::Init(ARGS&&... args)
	{
		using InitComponentsList = std::tuple<COMPONENTS...>;

		core::visit<sizeof...(COMPONENTS)>([&](auto component_index)
		{
			//Capture the type of the component in the list of component to init
			using ComponentType = typename std::tuple_element<component_index.value, InitComponentsList>::type;
			
			//Get the memory in the database, we need to find the index for this component in the database
			void* data = internal::GetComponentData(DATABASE_DECLARATION::s_database, m_indirection_index, DATABASE_DECLARATION::template ComponentIndex<ComponentType>());

			//Placement new
			new (data) ComponentType(std::forward<ARGS>(args)...);
		});

		return *this;
	}
	template<typename DATABASE_DECLARATION>
	template<typename COMPONENT>
	constexpr inline bool Instance<DATABASE_DECLARATION>::Contains()
	{
		EntityTypeMask entity_type_mask = internal::GetInstanceTypeMask(DATABASE_DECLARATION::s_database, m_indirection_index);
		return (DATABASE_DECLARATION::template ComponentMask<COMPONENT> & entity_type_mask) != 0;
	}

	template<typename DATABASE_DECLARATION>
	template<typename COMPONENT>
	constexpr inline COMPONENT& Instance<DATABASE_DECLARATION>::Get()
	{
		void* data = internal::GetComponentData(DATABASE_DECLARATION::s_database, m_indirection_index, DATABASE_DECLARATION::template ComponentIndex<COMPONENT>());

		return *reinterpret_cast<COMPONENT*>(data);
	}
}

#endif //ENTITY_COMPONENT_INSTANCE_H_
