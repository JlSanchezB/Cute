//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a simple pool
//////////////////////////////////////////////////////////////////////////

#ifndef SIMPLE_POOL_H_
#define SIMPLE_POOL_H_

#include <array>
#include <cassert>

namespace core
{
	template <typename DATA, size_t SIZE>
	class SimplePool
	{
		static constexpr size_t kInvalidIndex = -1;

		//Data storage of our DATA and free list
		using DataStorage = union
		{
			typename std::aligned_storage<sizeof(DATA), alignof(DATA)>::type data;
			typename size_t next_free_slot;
		};

		//List of free slots
		size_t m_first_free_allocated;

		//Vector of the data associated to this pool
		std::array<DataStorage, SIZE> m_data;

	public:
		SimplePool()
		{
			//Create all empty 
			for (size_t i = 0; i < SIZE; i++)
			{
				if (i != SIZE - 1)
				{
					m_data[i].next_free_slot = i + 1;
				}
				else
				{
					m_data[i].next_free_slot = kInvalidIndex;
				}
			}

			m_first_free_allocated = 0;
		}

		~SimplePool()
		{
			//Delete allocated ones
			Visit([](DATA& data)
			{
				//Destroy DATA
				(&data)->~DATA();
			});
		}

		template<typename VISITOR>
		void Visit(VISITOR&& visit_item)
		{
			std::vector<bool> allocated;
			allocated.resize(m_data.size());

			for (auto& it : allocated) it = true;

			size_t free_index = m_first_free_allocated;
			while (free_index != kInvalidIndex)
			{
				allocated[free_index] = false;
				free_index = m_data[free_index].next_free_slot;
			}

			for (size_t i = 0; i < allocated.size(); ++i)
			{
				//Check if it is allocated
				if (allocated[i])
				{
					//Visit data
					visit_item(*(reinterpret_cast<DATA*>(&m_data[i])));
				}
			}
		}

		template<typename ...Args>
		DATA* Alloc(Args && ...args)
		{
			if (m_first_free_allocated == kInvalidIndex)
			{
				//Error
				std::runtime_error::exception("Simple pool is full");
			}

			//Get next free slot
			size_t next_free_slot = m_data[m_first_free_allocated].next_free_slot;

			//Create DATA
			DATA* data = new(&m_data[m_first_free_allocated]) DATA(std::forward<Args>(args)...);

			m_first_free_allocated = next_free_slot;

			return data;
		}

		void Free(DATA* data)
		{
			//Find the index
			size_t index = data - reinterpret_cast<DATA*>(m_data.data());
			assert(index < m_data.size());

			//Free DATA
			(reinterpret_cast<DATA*>(&m_data[index]))->~DATA();

			//Add as free slot
			if (m_first_free_allocated == kInvalidIndex)
			{
				m_first_free_allocated = index;
			}
			else
			{
				m_data[index].next_free_slot = m_first_free_allocated;
				m_first_free_allocated = index;
			}
		}
	};
}

#endif
