//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a pool of generic handles
//////////////////////////////////////////////////////////////////////////

#ifndef HANDLE_POOL_H_
#define HANDLE_POOL_H_

#include <vector>
#include <algorithm>

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

		//Private constructor of a handle, only a pool can create valid handles
		Handle(TYPE index) : m_index(index)
		{
		}
		
		template<typename HANDLE, typename DATA>
		friend class HandlePool;

	public:
		//Public constructor
		Handle() : m_index(kInvalid)
		{
		}
	};

	//Pool of resources
	template<typename HANDLE, typename DATA>
	class HandlePool
	{
		static_assert(std::is_default_constructible<DATA>::value);

	public:
		//Init pool with a list of free slots avaliable
		void Init(size_t max_size, size_t init_size);

		//Allocate a handle
		template<typename ...Args>
		HANDLE Alloc(Args&&... args);

		//Free unused handle
		void Free(HANDLE& handle);

		//Accessors
		DATA& operator[](HANDLE handle)
		{
			return *reinterpret_cast<DATA*>(&m_data[handle.m_index]);
		}

		const DATA& operator[](const HANDLE handle) const
		{
			return *reinterpret_cast<const DATA*>(&m_data[handle.m_index]);
		}

		size_t GetIndex(const HANDLE handle) const
		{
			return handle.m_index;
		}

	private:
		typename HANDLE::type_param& GetNextFreeSlot(const typename HANDLE::type_param& index)
		{
			return *reinterpret_cast<typename HANDLE::type_param*>(&m_data[index]);
		}

		void GrowDataStorage(size_t new_size);

		//Max size the pool can grow
		size_t m_max_size;

		//Data storage of our DATA and free list
		using DataStorage = typename std::aligned_storage<std::max(sizeof(DATA), sizeof(typename HANDLE::type_param)), alignof(DATA)>::type;

		//List of free slots
		typename HANDLE::type_param m_first_free_allocated;

		//Vector of the data associated to this pool
		std::vector<DataStorage> m_data;

	};

	//Init pool with a list of free slots avaliable
	template<typename HANDLE, typename DATA>
	inline void HandlePool<HANDLE, DATA>::Init(size_t max_size, size_t init_size)
	{
		//assert(max_size < std::numeric_limits<typename HANDLE::type_param>::max() - 1);
		m_max_size = max_size;
		m_first_free_allocated = -1;

		GrowDataStorage(init_size);
	}


	//Allocate a handle
	template<typename HANDLE, typename DATA>
	template<typename ...Args>
	inline HANDLE HandlePool<HANDLE, DATA>::Alloc(Args && ...args)
	{
		//If there is free slots, it will use the last one free
		if (m_first_free_allocated == -1)
		{
			//Needs to allocate more
			size_t old_size = m_data.size();
			size_t new_size = std::min(m_max_size, old_size * 2);

			if (old_size < new_size)
			{
				GrowDataStorage(new_size);
			}
			else
			{
				//No more free handles, error
				return HANDLE(HANDLE::kInvalid);
			}
		}

		//Our handle will be allocated in m_first_free_allocated
		auto handle_slot = m_first_free_allocated;
		//Get next free slot
		m_first_free_allocated = GetNextFreeSlot(m_first_free_allocated);

		//Create DATA
		new(&m_data[handle_slot]) DATA(std::forward<Args>(args)...);

		return HANDLE(handle_slot);
	}

	//Free unused handle
	template<typename HANDLE, typename DATA>
	inline void HandlePool<HANDLE, DATA>::Free(HANDLE & handle)
	{
		//Destroy DATA
		(reinterpret_cast<DATA*>(&m_data[handle.m_index]))->~DATA();

		//Add it in the free list
		if (m_first_free_allocated == -1)
		{
			m_first_free_allocated = handle.m_index;
		}
		else
		{
			GetNextFreeSlot(handle.m_index) = m_first_free_allocated;
			m_first_free_allocated = handle.m_index;
		}
		//Reset handle to an invalid, it will avoid it keep the index
		handle.m_index = HANDLE::kInvalid;
	}

	template<typename HANDLE, typename DATA>
	inline void HandlePool<HANDLE, DATA>::GrowDataStorage(size_t new_size)
	{
		//assert(m_first_free_allocated == 1);

		//Old size
		size_t old_size = m_data.size();

		//Reserve the data
		m_data.resize(new_size);

		//Fill all the free slots
		for (size_t i = old_size; i < new_size; ++i)
		{
			typename HANDLE::type_param& next_free_slot = *reinterpret_cast<typename HANDLE::type_param*>(&m_data[i]);

			if (i < (new_size - 1))
			{
				next_free_slot = static_cast<typename HANDLE::type_param>(i + 1);
			}
			else
			{
				//Close linked list
				next_free_slot = -1;
			}
		}

		m_first_free_allocated = static_cast<typename HANDLE::type_param>(old_size);
	}

}

#endif //HANDLE_POOL_H_