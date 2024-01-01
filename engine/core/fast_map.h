//////////////////////////////////////////////////////////////////////////
// Cute engine - Fast map, fast implementation of a map for small quantity of items
//////////////////////////////////////////////////////////////////////////
#ifndef FAST_MAP_h
#define FAST_MAP_h

#include <vector>
#include <array>
#include <cassert>

namespace core
{
	//Flat linear map implementation
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
				assert(m_data);
				return m_data;
			}

			DATA& operator*()&
			{
				assert(m_data);
				return *m_data;
			}

			DATA&& operator*()&&
			{
				assert(m_data);
				return *m_data;
			}

			explicit operator bool() const
			{
				return m_data != nullptr;
			}

			Accesor(DATA* data) : m_data(data)
			{
			}

			Accesor<DATA> operator=(DATA& data)
			{
				assert(m_data);
				m_data = data;
				return *this;
			}

		private:
			DATA* m_data;
		};


		template<typename KEY, typename DATA>
		class Iterator
		{
		public:
			Iterator(FastMap* fast_map, size_t index) :
				m_fast_map(fast_map), m_index(index)
			{
			}

			std::pair<KEY&, DATA&> operator*()
			{
				return std::pair<KEY&, DATA&>(m_fast_map->m_key[m_index], m_fast_map->m_data[m_index]);
			}

			bool operator!= (const Iterator & other) const
			{
				return m_fast_map != other.m_fast_map || m_index != other.m_index;
			}

			const Iterator& operator++ ()
			{
				assert(m_index != kInvalid);

				m_index++;
				//Just go to capacity looking for a reserved space
				while (m_index < m_fast_map->m_capacity * m_fast_map->m_bucket_size && !m_fast_map->m_reserved[m_index])
				{
					m_index++;
				}

				if (m_index < m_fast_map->m_capacity * m_fast_map->m_bucket_size)
				{
					//Good
					return *this;
				}
				else
				{
					//End
					m_index = kInvalid;
					return *this;
				}
			}

		private:
			FastMap* m_fast_map;
			size_t m_index;
		};

		//Insert data
		template<typename SOURCE_DATA>
		Accesor<DATA> Insert(const KEY& key, SOURCE_DATA&& data);

		//Access data
		Accesor<DATA> Find(const KEY& key);
		Accesor<const DATA> Find(const KEY& key) const;

		Accesor<const DATA> operator[](const KEY& key) const
		{
			return Find(key);
		}

		Accesor<DATA> operator[](const KEY& key)
		{
			return Find(key);
		}

		Iterator<KEY,DATA> begin()
		{
			//Look for the first item reserved
			size_t index = 0;
			while (index < m_capacity * m_bucket_size && !m_reserved[index])
			{
				index++;
			}
			if (index < m_capacity * m_bucket_size)
			{
				//Good
				return Iterator<KEY, DATA>(this, index);
			}
			else
			{
				//End
				return Iterator<KEY, DATA>(this, kInvalid);
			}
		}

		Iterator<KEY, DATA> end()
		{
			return Iterator<KEY, DATA>(this, kInvalid);
		}

		//Visit
		template<typename VISITOR>
		void Visit(VISITOR&& visitor)
		{
			for (auto& it : *this)
			{
				visitor(it.second);
			}
		}

		template<typename VISITOR>
		void VisitNamed(VISITOR&& visitor)
		{
			for (auto& it : *this)
			{
				visitor(it.first, it.second);
			}
		}

		//Clear
		void clear()
		{
			*this = {};
		}

		//Size (not really fast)
		size_t size() const
		{
			return m_size;
		}

		FastMap(size_t start_capacity = 8, size_t bucket_size = 3)
		{
			//Init with correct values
			m_bucket_size = bucket_size;
			m_capacity = 0;
			m_size = 0;

			//Init to the start capacity
			Grow(start_capacity);
		}

	private:
		
		//Array of bits to indicate if the slot is reserved
		std::vector<bool> m_reserved;
		//Array of keys
		std::vector<KEY> m_key;
		//Array of data
		std::vector<DATA> m_data;

		//Capacity (needs to be power of 2)
		size_t m_capacity;

		//Size
		size_t m_size;

		//Bucket size
		size_t m_bucket_size;

		//Return two indices
		//First one is the index of the key and kInvald if doesn't find it
		//Second one is the slot that the key can be added if it was not found
		std::pair<size_t, size_t> GetIndex(const KEY& key);

		constexpr static size_t kInvalid = static_cast<size_t>(-1);

		//Grow by new capacity
		void Grow(size_t new_capacity)
		{
			//Move current data into a swap buffers
			std::vector<bool> source_reserved = std::move(m_reserved);
			std::vector<KEY> source_key = std::move(m_key);
			std::vector<DATA> source_data = std::move(m_data);
			size_t source_capacity = m_capacity;
			size_t old_size = m_size;

			//Define the new capacity
			m_reserved.resize(new_capacity * m_bucket_size);
			m_key.resize(new_capacity * m_bucket_size);
			m_data.resize(new_capacity * m_bucket_size);
			m_capacity = new_capacity;
			m_size = 0;

			//Add old key/data into the new map
			for (size_t i = 0; i < source_capacity * m_bucket_size; ++i)
			{
				if (source_reserved[i])
				{
					//Insert
					Insert(std::move(source_key[i]), std::move(source_data[i]));
				}
			}

			//Done
			assert(old_size == m_size);
		}
	};

	template<typename KEY, typename DATA>
	inline std::pair<size_t, size_t> FastMap<KEY, DATA>::GetIndex(const KEY& key)
	{
		size_t search_slot = 0;
		
		//The begin search slot is calculated from the hash value
		search_slot = (std::hash<KEY>{}(key) & (m_capacity - 1)) * m_bucket_size;
		size_t count = 0;
		while (m_reserved[search_slot])
		{
			if (m_key[search_slot] == key)
			{
				//Found
				return std::make_pair(search_slot, kInvalid);
			}
			count++;
			assert(count < m_capacity * m_bucket_size);

			search_slot++;
			search_slot = search_slot % (m_capacity * m_bucket_size); //The flat map is really a ring buffer
		}

		//Not found
		return std::make_pair(kInvalid, search_slot);
	}

	template<typename KEY, typename DATA>
	template<typename SOURCE_DATA>
	inline FastMap<KEY, DATA>::Accesor<DATA> FastMap<KEY, DATA>::Insert(const KEY& key, SOURCE_DATA&& data)
	{
		auto index = GetIndex(key);

		if (index.first == kInvalid)
		{
			//Add

			//Check if we need to grow
			if (m_size + 1 >= m_capacity)
			{
				//Needs to grow
				Grow(m_capacity * 2);

				//Get other index in the new map
				index = GetIndex(key);
			}
			m_size++;
			m_reserved[index.second] = true;
			m_key[index.second] = key;
			m_data[index.second] = std::forward<DATA>(data);

			return Accesor<DATA>(&m_data[index.second]);
		}
		else
		{
			//It was already added, just update
			m_key[index.first] = key;
			m_data[index.first] = std::forward<DATA>(data);

			return Accesor<DATA>(&m_data[index.first]);
		}
	}

	template<typename KEY, typename DATA>
	inline FastMap<KEY, DATA>::Accesor<const DATA> FastMap<KEY, DATA>::Find(const KEY& key) const
	{
		auto index = GetIndex(key);

		if (index.first != kInvalid)
		{
			return Accesor<const DATA>(&m_data[index.first]);
		}
		else
		{
			//Invalid
			return Accesor<const DATA>(nullptr);
		}
	}

	template<typename KEY, typename DATA>
	inline FastMap<KEY, DATA>::Accesor<DATA> FastMap<KEY, DATA>::Find(const KEY& key)
	{
		auto index = GetIndex(key);

		if (index.first != kInvalid)
		{
			return Accesor<DATA>(&m_data[index.first]);
		}
		else
		{
			//Invalid
			return Accesor<DATA>(nullptr);
		}
	}
}

#endif //FAST_MAP_h
