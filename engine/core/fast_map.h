//////////////////////////////////////////////////////////////////////////
// Cute engine - Fast map, fast implementation of a map for small quantity of items
//////////////////////////////////////////////////////////////////////////
#ifndef FAST_MAP_h
#define FAST_MAP_h

#include <vector>
#include <array>


namespace core
{
	//Lineal probe map implemetation
	template <typename KEY, typename DATA, size_t NUM_BUCKETS = 1>
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

		//Visit
		template<typename VISITOR>
		void Visit(VISITOR&& visitor)
		{
			for (auto& bucket : m_buckets)
			{
				for (auto& data : bucket.m_data)
				{
					visitor(data);
				}
			}
		}

		template<typename VISITOR>
		void VisitNamed(VISITOR&& visitor)
		{
			for (auto& bucket : m_buckets)
			{
				size_t count = bucket.m_data.size();
				for (size_t i = 0; i < count; ++i)
				{
					visitor(bucket.m_key[i], bucket.m_data[i]);
				}
			}
		}

		//Clear
		void clear()
		{
			for (auto& bucket : m_buckets)
			{
				bucket.m_key.clear();
				bucket.m_data.clear();
			}
		}

	private:
		struct Bucket
		{
			//Lineal search inside a vector of keys
			std::vector<KEY> m_key;
			//Vector of data
			std::vector<DATA> m_data;
		};

		std::array<Bucket, NUM_BUCKETS> m_buckets;

		std::pair<size_t,size_t> GetIndex(const KEY& key) const;
		constexpr static size_t kInvalid = static_cast<size_t>(-1);
	};

	template<typename KEY, typename DATA, size_t NUM_BUCKETS>
	inline std::pair<size_t, size_t> FastMap<KEY, DATA, NUM_BUCKETS>::GetIndex(const KEY& key) const
	{
		size_t bucket_index;
		if constexpr (NUM_BUCKETS == 1)
		{
			bucket_index = 0;
		}
		else
		{
			bucket_index = key & (NUM_BUCKETS - 1);
		}

		auto& bucket = m_buckets[bucket_index];
		const size_t count = bucket.m_key.size();
		for (size_t i = 0; i < count; ++i)
		{
			if (bucket.m_key[i] == key)
			{
				return std::make_pair(bucket_index, i);
			}
		}

		return std::make_pair(bucket_index, kInvalid);
	}

	template<typename KEY, typename DATA, size_t NUM_BUCKETS>
	template<typename SOURCE_DATA>
	inline FastMap<KEY, DATA, NUM_BUCKETS>::Accesor<DATA> FastMap<KEY, DATA, NUM_BUCKETS>::Set(const KEY& key, SOURCE_DATA&& data)
	{
		auto index = GetIndex(key);

		if (index.second == kInvalid)
		{
			Bucket& bucket = m_buckets[index.first];
			bucket.m_key.push_back(key);
			bucket.m_data.emplace_back(std::forward<DATA>(data));
			return Accesor<DATA>(&bucket.m_data.back());
		}
		else
		{
			Bucket& bucket = m_buckets[index.first];
			bucket.m_data[index.first] = std::forward<DATA>(data);
			return Accesor<DATA>(&bucket.m_data[index.second]);
		}
	}

	template<typename KEY, typename DATA, size_t NUM_BUCKETS>
	inline FastMap<KEY, DATA, NUM_BUCKETS>::Accesor<const DATA> FastMap<KEY, DATA, NUM_BUCKETS>::Get(const KEY& key) const
	{
		auto index = GetIndex(key);

		if (index.second != kInvalid)
		{
			return Accesor<const DATA>(&m_buckets[index.first].m_data[index.second]);
		}
		else
		{
			//Invalid
			return Accesor<const DATA>(nullptr);
		}
	}

	template<typename KEY, typename DATA, size_t NUM_BUCKETS>
	inline FastMap<KEY, DATA, NUM_BUCKETS>::Accesor<DATA> FastMap<KEY, DATA, NUM_BUCKETS>::Get(const KEY& key)
	{
		auto index = GetIndex(key);

		if (index.second != kInvalid)
		{
			return Accesor<DATA>(&m_buckets[index.first].m_data[index.second]);
		}
		else
		{
			//Invalid
			return Accesor<DATA>(nullptr);
		}
	}
}

#endif //FAST_MAP_h
