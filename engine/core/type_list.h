//////////////////////////////////////////////////////////////////////////
// Cute engine - Compile time type list container
//////////////////////////////////////////////////////////////////////////

#ifndef VIRTUAL_ALLOC_H_
#define VIRTUAL_ALLOC_H_

#include <tuple>

namespace core
{
	template<typename FIND_ELEMENT, size_t current_index, typename LAST_ELEMENT>
	constexpr size_t find_index()
	{
		if (std::is_same<FIND_ELEMENT, LAST_ELEMENT>::value)
		{
			return current_index;
		}
		else
		{
			return -1;
		}
	};

	template<typename FIND_ELEMENT, size_t current_index, typename ELEMENT1, typename ELEMENT2, typename ...REST_ELEMENTS>
	constexpr size_t find_index()
	{
		if (std::is_same<FIND_ELEMENT, ELEMENT1>::value)
		{
			return current_index;
		}
		else
		{
			return find_index<FIND_ELEMENT, current_index + 1, ELEMENT2, REST_ELEMENTS...>();
		}
	};

	template<typename FUNCTION, typename LAST_ELEMENT>
	constexpr void visit()
	{
		FUNCTION::template Visit<LAST_ELEMENT>();
	};

	template<typename FUNCTION, typename ELEMENT1, typename ELEMENT2, typename ...REST_ELEMENTS>
	constexpr void visit()
	{
		FUNCTION::template Visit<ELEMENT1>();
		visit<FUNCTION, ELEMENT2, REST_ELEMENTS...>();
	};

	template <typename ...ELEMENTS>
	struct TypeList
	{
		template<size_t INDEX>
		using ElementType = typename std::tuple_element<INDEX, std::tuple<ELEMENTS...>>::type;

		constexpr static size_t Size()
		{
			return sizeof...(ELEMENTS);
		}

		template<typename ELEMENT>
		constexpr static size_t ElementIndex()
		{
			return find_index<ELEMENT, 0, ELEMENTS...>();
		}

		template<typename FUNCTION>
		constexpr static void Visit()
		{
			visit<FUNCTION, ELEMENTS...>();
		}
	};
}

#endif //VIRTUAL_ALLOC_H_