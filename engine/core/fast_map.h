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
			DATA* operator->()
			{
				return m_data;
			}

			DATA& operator*()&
			{
				return *m_data;
			}

			DATA&& operator*()&&
			{
				return *m_data;
			}

			explicit operator bool() const
			{
				return m_data != nullptr;
			}

			Accesor(DATA* data) : m_data(data)
			{
			}

		private:
			DATA* m_data;
		};

		//Set data
		template<typename SOURCE_DATA>
		Accesor<DATA> Set(const KEY& key, SOURCE_DATA&& data);

		//Access data
		Accesor<DATA> Get(const KEY& key);
		Accesor<const DATA> Get(const KEY& key) const;

		Accesor<const DATA> operator[](const KEY& key) const
		{
			return Get(key);
		}

		Accesor<DATA> operator[](const KEY& key)
		{
			return Get(key);
		}


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
	template<typename SOURCE_DATA>
	inline FastMap<KEY, DATA>::Accesor<DATA> FastMap<KEY, DATA>::Set(const KEY& key, SOURCE_DATA&& data)
	{
		size_t index = GetIndex(key);

		if (index == kInvalid)
		{
			m_key.push_back(key);
			m_data.emplace_back(std::forward<DATA>(data));
			return Accesor<DATA>(&m_data.back());
		}
		else
		{
			m_data[index] = std::forward<DATA>(data);
			return Accesor<DATA>(&m_data[index]);
		}
	}

	template<typename KEY, typename DATA>
	inline FastMap<KEY, DATA>::Accesor<const DATA> FastMap<KEY, DATA>::Get(const KEY& key) const
	{
		size_t index = GetIndex(key);

		if (index != kInvalid)
		{
			return Accesor<const DATA>(&m_data[index]);
		}
		else
		{
			//Invalid
			return Accesor<const DATA>(nullptr);
		}
	}

	template<typename KEY, typename DATA>
	inline FastMap<KEY, DATA>::Accesor<DATA> FastMap<KEY, DATA>::Get(const KEY& key)
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
