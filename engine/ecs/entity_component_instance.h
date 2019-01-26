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
		void InitDefault();

		//Init Components with constructor
		template<typename ...COMPONENTS>
		Instance<DATABASE_DECLARATION>& Init();

		//Countains component
		template<typename COMPONENT>
		bool Contains();

		//Accessor to a component
		template<typename COMPONENT>
		COMPONENT& Get();

	private:
		InstanceIndirectionIndexType m_indirection_index;

		template<typename DATABASE_DECLARATION, typename ENTITY_TYPE>
		friend Instance<DATABASE_DECLARATION> AllocInstance();
	};

	template<typename DATABASE_DECLARATION>
	inline void Instance<DATABASE_DECLARATION>::InitDefault()
	{
		//Calls the default contructor for all components of the instance
		size_t instance_type_index = internal::GetInstanceType(DATABASE_DECLARATION::s_database, m_indirection_index);

	}
	template<typename DATABASE_DECLARATION>
	template<typename ...COMPONENTS>
	inline Instance<DATABASE_DECLARATION>& Instance<DATABASE_DECLARATION>::Init()
	{
		
		return *this;
	}
	template<typename DATABASE_DECLARATION>
	template<typename COMPONENT>
	inline bool Instance<DATABASE_DECLARATION>::Contains()
	{
		EntityTypeMask entity_type_mask = internal::GetInstanceTypeMask(DATABASE_DECLARATION::s_database, m_indirection_index);
		return (DATABASE_DECLARATION::EntityTypes::template EntityTypeMask<COMPONENT> & entity_type_mask) != 0;
	}

	template<typename DATABASE_DECLARATION>
	template<typename COMPONENT>
	inline COMPONENT& Instance<DATABASE_DECLARATION>::Get()
	{
		void* data = internal::GetComponentData(DATABASE_DECLARATION::s_database, m_indirection_index, DATABASE_DECLARATION::template ComponentIndex<COMPONENT>());

		return *reinterpret_cast<COMPONENT*>(data);
	}
}

#endif //ENTITY_COMPONENT_INSTANCE_H_
