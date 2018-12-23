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

		while(data[0] != '\0')
		{
			uint32_t value = static_cast<uint32_t>(data[0]);
			hash = hash ^ value;
			hash *= prime;
			data++;
		}

		return hash;
	}

	constexpr auto lo(uint64_t x)
		-> uint64_t
	{
		return x & uint32_t(-1);
	}

	constexpr auto hi(uint64_t x)
		-> uint64_t
	{
		return x >> 32;
	}

	constexpr auto mulu64(uint64_t a, uint64_t b)
		-> uint64_t
	{
		return 0
			+ (lo(a)*lo(b) & uint32_t(-1))
			+ ((((( hi(lo(a)*lo(b)) + lo(a)*hi(b)) & uint32_t(-1)) + hi(a)*lo(b)) & uint32_t(-1)) << 32);
	}

	inline constexpr uint64_t hash_64_fnv1a(const char* data)
	{
		uint64_t hash = 0xcbf29ce484222325ULL;
		uint64_t prime = 0x100000001b3ULL;

		while (data[0] != '\0')
		{
			uint64_t value = static_cast<uint64_t>(data[0]);
			hash = hash ^ value;
			hash = mulu64(hash, prime);
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

	template<typename TYPE>
	class HashConst
	{
		TYPE m_hash;
	public:
		constexpr HashConst(TYPE value)
		{
			m_hash = value;
		}
		TYPE GetHash() const
		{
			return m_hash;
		}
	};

	//String hash
	template<uint64_t NAMESPACE, typename TYPE>
	class StringHashT
	{
		TYPE m_hash;
	public:
		constexpr explicit StringHashT(const char* data)
		{
			m_hash = calculate_hash<TYPE>(data);
		}
		constexpr explicit StringHashT(const HashConst<TYPE>& data)
		{
			m_hash = data.GetHash();
		}

		TYPE GetHash() const
		{
			return m_hash;
		}

		bool operator==(const StringHashT<NAMESPACE, TYPE>& b) const
		{
			return m_hash == b.m_hash;
		}

		bool operator==(const HashConst<TYPE>& b) const
		{
			return m_hash == b.GetHash();
		}
	};
}

template<uint64_t NAMESPACE>
using StringHash16 = core::StringHashT<NAMESPACE, uint16_t>;

template<uint64_t NAMESPACE>
using StringHash32 = core::StringHashT<NAMESPACE, uint32_t>;

template<uint64_t NAMESPACE>
using StringHash64 = core::StringHashT<NAMESPACE, uint64_t>;

core::HashConst<uint16_t> constexpr operator "" _sh16(const char* str, size_t)
{
	return core::HashConst<uint16_t>(core::calculate_hash<uint16_t>(str));
}

core::HashConst<uint32_t> constexpr operator "" _sh32(const char* str, size_t)
{
	return core::HashConst<uint32_t>(core::calculate_hash<uint32_t>(str));
}

core::HashConst<uint64_t> constexpr operator "" _sh64(const char* str, size_t)
{
	return core::HashConst<uint64_t>(core::calculate_hash<uint64_t>(str));
}

uint64_t constexpr operator "" _namespace(const char* str, size_t)
{
	return core::calculate_hash<uint64_t>(str);
}


namespace std
{
	template<uint64_t NAMESPACE, typename TYPE>
	struct hash<core::StringHashT<NAMESPACE, TYPE>>
	{
		size_t operator()(core::StringHashT<NAMESPACE, TYPE> const& s) const
		{
			return static_cast<size_t>(s.GetHash());
		}
	};
}

#endif


