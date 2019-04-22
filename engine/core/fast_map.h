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
	template <typename KEY, typename DATA>
	class FastMap
	{
	public:

		//Accesor helper
		template<typename DATA>
		class Accesor
		{
		public:
			const DATA* operator->()  const
			{
				return m_data;
			}

			const DATA& operator*()  const&
			{
				return *m_data;
			}

			const DATA&& operator*()  const&&
			{
				return *m_data;
			}

			explicit operator bool() const
			{
				return m_data != nullptr;
			}

			Accesor(const DATA* data) : m_data(data)
			{
			}

		private:
			const DATA* m_data;
		};

		//Set data
		void Set(const KEY& key, DATA&& data);

		//Access data
		const Accesor<DATA> operator[](const KEY& key) const;

		//Iterators begin and end
		auto begin() const
		{
			return m_data.begin();
		}
		auto end() const
		{
			return m_data.end();
		}

		//Clear
		void clear()
		{
			m_key.clear();
			m_data.clear();
		}

	private:
		//Lineal search inside a vector of keys
		std::vector<KEY> m_key;
		//Vector of data
		std::vector<DATA> m_data;

		size_t GetIndex(const KEY& key) const;
		constexpr static size_t kInvalid = static_cast<size_t>(-1);
	};

	template<typename KEY, typename DATA>
	inline size_t FastMap<KEY, DATA>::GetIndex(const KEY& key) const
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

	template<typename KEY, typename DATA>
	inline void FastMap<KEY, DATA>::Set(const KEY& key, DATA&& data)
	{
		size_t index = GetIndex(key);

		if (index == kInvalid)
		{
			m_key.push_back(key);
			m_data.emplace_back(std::forward<DATA>(data));

		}
		else
		{
			m_data[index] = std::forward<DATA>(data);
		}
	}

	template<typename KEY, typename DATA>
	inline const FastMap<KEY, DATA>::Accesor<DATA> FastMap<KEY, DATA>::operator[](const KEY& key) const
	{
		size_t index = GetIndex(key);

		if (index != kInvalid)
		{
			return Accesor<DATA>(&m_data[index]);
		}
		else
		{
			//Invalid
			return Accesor<DATA>(nullptr);
		}
	}
}

#endif //FAST_MAP_h
