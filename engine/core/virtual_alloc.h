//////////////////////////////////////////////////////////////////////////
// Cute engine - Virtual alloc and deallocation memory
//////////////////////////////////////////////////////////////////////////

#ifndef VIRTUAL_ALLOC_H_
#define VIRTUAL_ALLOC_H_

#include <stdint.h>
#include <type_traits>

template <typename Enum, typename std::enable_if_t<std::is_enum<Enum>::value, int> = 0>
constexpr inline Enum operator| (Enum lhs, Enum rhs)
{
	return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) | static_cast<std::underlying_type_t<Enum>>(rhs));
}

template <typename Enum, typename std::enable_if_t<std::is_enum<Enum>::value, int> = 0>
constexpr inline bool check_flag(Enum lhs, Enum rhs)
{
	return (static_cast<std::underlying_type_t<Enum>>(lhs) & static_cast<std::underlying_type_t<Enum>>(rhs)) != 0;
}

namespace core
{
	enum class AllocFlags
	{
		Reserve = 1 << 0,
		Commit = 1 << 1
	};

	enum class FreeFlags
	{
		Decommit = 1 << 0,
		Release = 1 << 1
	};

	void* VirtualAlloc(void* ptr, size_t size, AllocFlags flags);
	void VirtualFree(void* ptr, size_t size, FreeFlags flags);
	size_t GetPageSize();
}

#endif //VIRTUAL_ALLOC_H_
