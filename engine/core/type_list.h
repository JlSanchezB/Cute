//////////////////////////////////////////////////////////////////////////
// Cute engine - Compile time type list container
//////////////////////////////////////////////////////////////////////////

#ifndef TYPE_LIST_H_
#define TYPE_LIST_H_

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
	};

	//Helpers for iterating variadic templates lists
	template<std::size_t N>
	struct num { static const constexpr auto value = N; };

	template <class F, std::size_t... Is>
	constexpr inline void visit(F&& func, const std::index_sequence<Is...>&)
	{
		using expander = int[];
		(void)expander {
			0, ((void)func(num<Is>{}), 0)...
		};
	}

	template <std::size_t N, typename F>
	constexpr inline void visit(F&& func)
	{
		visit(func, std::make_index_sequence<N>());
	}
}

#endif //TYPE_LIST_H_