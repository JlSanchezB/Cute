//////////////////////////////////////////////////////////////////////////
// Cute engine - Fast map, fast implementation of a map for small quantity of items
//////////////////////////////////////////////////////////////////////////
#ifndef FAST_MAP_h
#define FAST_MAP_h

#include <vector>
#include <optional>

namespace core
{
	//Lineal probe map implemetation
	template <typename Key, typename DATA>
	class FastMap
	{
		//Set data
		void Set(const Key& key, const DATA& data);

		//Access data
		const std::optional<DATA>& operator[](const Key& key) const;
	private:
		//Lineal search inside a vector of keys
		std::vector<KEY> m_key;
		//Vector of data
		std::vector<DATA> m_data;

		size_t GetIndex(const Key& key);
		constexpr static size_t kInvalid = static_cast<size_t>(-1);
	};

	template<typename Key, typename DATA>
	inline size_t FastMap<Key, DATA>::GetIndex(const Key& key)
	{
		const size_t count = m_key.size();
		for (size_t i = 0; i < count; ++i)
		{
			if (m_key[i] == key)
			{
				return i;
			}
		}

		return kInvalid;
	}

	template<typename Key, typename DATA>
	inline void FastMap<Key, DATA>::Set(const Key& key, const DATA& data)
	{
		size_t index = GetIndex(key);

		if (index == kInvalid)
		{
			m_key.push_back(key);
			m_data.push_back(data);

		}
		else
		{
			m_data[index] = data;
		}
	}

	template<typename Key, typename DATA>
	inline const std::optional<DATA>& FastMap<Key, DATA>::operator[](const Key& key) const
	{
		size_t index = GetIndex(key);

		if (index != kInvalid)
		{
			return std::make_optional(m_data[index]);
		}
		else
		{
			//Invalid
			return {};
		}
	}
}

#endif //FAST_MAP_h
