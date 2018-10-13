//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a pool of generic handles
//////////////////////////////////////////////////////////////////////////

#ifndef HANDLE_POOL_H_
#define HANDLE_POOL_H_

#include <vector>

namespace core
{
	//Handle of a resource, specialised by a enum TYPE and the size
	template <typename ENUM, typename TYPE>
	class Handle
	{
		using type_param = TYPE;

		//Index inside the handlepool to access the data associated to this handle 
		TYPE m_index;

		//Invalid handle
		static const TYPE kInvalid = -1;

		//Private constructor of a handle
		Handle(TYPE index) : m_index(index)
		{
		}

		friend class HandlePool;
	};

	//Pool of resources
	template<class HANDLE, typename DATA>
	class HandlePool
	{
		static_assert(std::is_default_constructible<DATA>::value);

	public:
		//Init pool with a list of free slots avaliable
		void Init(size_t max_size, size_t init_size)
		{
			assert(max_size < std::numeric_limits<HANDLE::type_param>::max() - 1);
			m_max_size = max_size;

			//Reserve the data
			m_data.resize(init_size);

			//Create all as free slots
			m_free_slots.resize(init_size);
			for (size_t i = 0; i <= init_size; ++i)
			{
				//Add slots in reverse, so it will allocate first the begining of the pool
				m_free_slots[i] = init_size - i - 1;
			}

		}

		//Allocate a handle
		HANDLE Alloc()
		{
			//If there is free slots, it will use the last one free
			if (m_free_slots.size() > 0)
			{
				return HANDLE(m_free_slots.pop_back());
			}
			else
			{
				//Needs to allocate more
				size_t old_size = m_data.size();
				//We duplicate the size
				size_t new_allocate_size = old_size;
				size_t new_size = std::min(m_max_size, old_size * 2);
				
				if (old_size < new_size)
				{
					//Reserve the data
					m_free_slots.resize(new_size);

					//Create the free slots
					m_free_slots.resize(new_allocate_size);
					for (size_t i = 0; i <= new_allocate_size; ++i)
					{
						//Add slots in reverse, so it will allocate first the begining of the pool
						m_free_slots[i] = old_size + new_allocate_size - i - 1;
					}

					return HANDLE(m_free_slots.pop_back());
				}
				else
				{
					//No more free handles, error
					return HANDLE(HANDLE::kInvalid);
				}
			}
		}

		//Free unused handle
		void Free(HANDLE handle)
		{
			//Just add the handle to the free slots
			m_free_slots.push_back(handle.m_index);
		}

		//Accessors
		DATA& operator[](HANDLE handle)
		{
			m_data[handle.m_index];
		}

		const DATA& operator[](HANDLE handle)
		{
			m_data[handle.m_index];
		}

	private:
		//Max size the pool can grow
		size_t m_max_size;

		//List of free slots
		std::vector<HANDLE.type_param> m_free_slots;

		//Vector of the data associated to this pool
		std::vector<DATA> m_data;

	};

}

#endif //HANDLE_POOL_H_