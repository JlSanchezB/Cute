//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for string hash, allows compile time hashes
// with collision/debugging in non release configurations
//////////////////////////////////////////////////////////////////////////

#ifndef STRING_HASH_H_
#define STRING_HASH_H_

#include <stdint.h>

namespace core
{
	inline constexpr uint32_t hash_32_fnv1a(const char* data)
	{
		uint32_t hash = 0x811c9dc5;
		uint32_t prime = 0x1000193;

		while(data[0] != 0)
		{
			uint8_t value = data[0];
			hash = hash ^ value;
			hash *= prime;
			data++;
		}

		return hash;
	}

	inline constexpr uint64_t hash_64_fnv1a(const char* data)
	{
		uint64_t hash = 0xcbf29ce484222325;
		uint64_t prime = 0x100000001b3;

		while (data[0] != 0)
		{
			uint8_t value = data[0];
			hash = hash ^ value;
			hash *= prime;
			data++;
		}

		return hash;
	}

	template<typename TYPE>
	inline constexpr TYPE calculate_hash(const char* data);

	template<>
	inline constexpr uint32_t calculate_hash(const char* data)
	{
		return hash_32_fnv1a(data);
	}

	template<>
	inline constexpr uint64_t calculate_hash(const char* data)
	{
		return hash_64_fnv1a(data);
	}

	template<>
	inline constexpr uint16_t calculate_hash(const char* data)
	{
		return static_cast<uint16_t>(hash_32_fnv1a(data));
	}

	//String hash
	template<typename TYPE>
	class StringHashT
	{
		TYPE m_hash;
	public:
		constexpr explicit StringHashT(const char* data)
		{
			m_hash = calculate_hash<TYPE>(data);
		}
		TYPE GetHash() const
		{
			return m_hash;
		}

		bool operator==(const StringHashT<TYPE>& b) const
		{
			return m_hash == b.m_hash;
		}
	};
}

using StringHash16 = core::StringHashT<uint16_t>;
using StringHash32 = core::StringHashT<uint32_t>;
using StringHash64 = core::StringHashT<uint64_t>;

StringHash16 constexpr operator "" _sh16(const char* str, size_t)
{
	return StringHash16(str);
}

StringHash32 constexpr operator "" _sh32(const char* str, size_t)
{
	return StringHash32(str);
}

StringHash64 constexpr operator "" _sh64(const char* str, size_t)
{
	return StringHash64(str);
}

namespace std
{
	template<typename TYPE>
	struct hash<core::StringHashT<TYPE>>
	{
		size_t operator()(core::StringHashT<TYPE> const& s) const
		{
			return static_cast<size_t>(s.GetHash());
		}
	};
}

#endif


