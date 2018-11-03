//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a pool of generic handles
//////////////////////////////////////////////////////////////////////////

#ifndef HANDLE_POOL_H_
#define HANDLE_POOL_H_

#include "log.h"

#include <vector>
#include <algorithm>
#include <cassert>

namespace core
{
	template <typename ENUM, typename TYPE>
	class HandleAccessor
	{
	protected:
		using type_param = TYPE;
		using enum_param = ENUM;

		//Index inside the handlepool to access the data associated to this handle 
		TYPE m_index;

		//Invalid handle
		static const TYPE kInvalid = -1;

		template<typename HANDLE, typename DATA>
		friend class HandlePool;

		HandleAccessor() : m_index(kInvalid)
		{
		}
		HandleAccessor(type_param index) : m_index(index)
		{
		}

	public:
		bool IsValid() const
		{
			return m_index != kInvalid;
		}
	};

	//Handles can only be created from a pool and they can not be copied, only moved
	template <typename ENUM, typename TYPE>
	class Handle : public HandleAccessor<ENUM, TYPE>
	{
		using Accessor = HandleAccessor<ENUM, TYPE>;

		//Private constructor of a handle, only a pool can create valid handles
		Handle(TYPE index) : HandleAccessor(index)
		{
		}

		template<typename HANDLE, typename DATA>
		friend class HandlePool;

		template<typename HANDLE, typename DATA>
		friend class WeakHandle;

	public:
		//Public constructor
		Handle()
		{
		}

		//Disable copy construction and copy assing, only is allowed to move

		Handle(const Handle& a) = delete;

		Handle(Handle&& a)
		{
			Accessor::m_index = a.m_index;
			a.m_index = Accessor::kInvalid;
		}

		Handle& operator=(const Handle& a) = delete;

		Handle& operator=(Handle&& a)
		{
			Accessor::m_index = a.m_index;
			a.m_index = Accessor::kInvalid;
			return *this;
		}

		~Handle()
		{
		}
	};

	//Weak handle only can be create from a handle
	//They can be copied
	template <typename ENUM, typename TYPE>
	class WeakHandle : public HandleAccessor<ENUM, TYPE>
	{
		using Accessor = HandleAccessor<ENUM, TYPE>;

	public:
		//Default constructor
		WeakHandle()
		{
		}

		//A weak handle can be created from a handle
		WeakHandle(const Handle<ENUM, TYPE>& handle)
		{
			Accessor::m_index = handle.m_index;
		}
	};

	//Pool of resources
	template<typename HANDLE, typename DATA>
	class HandlePool
	{
		static_assert(std::is_default_constructible<DATA>::value);
	protected:
		using Accessor = HandleAccessor<typename HANDLE::enum_param, typename HANDLE::type_param>;

	public:
		~HandlePool();

		//Init pool with a list of free slots avaliable
		void Init(size_t max_size, size_t init_size);

		//Allocate a handle
		template<typename ...Args>
		HANDLE Alloc(Args&&... args);

		//Free unused handle
		void Free(HANDLE& handle);

		//Accessors
		DATA& operator[](const Accessor& handle)
		{
			return *reinterpret_cast<DATA*>(&m_data[handle.m_index].data);
		}

		const DATA& operator[](const Accessor& handle) const
		{
			return *reinterpret_cast<const DATA*>(&m_data[handle.m_index].data);
		}

	private:
		typename HANDLE::type_param& GetNextFreeSlot(const typename HANDLE::type_param& index)
		{
			return m_data[index].next_free_slot;
		}

		void GrowDataStorage(size_t new_size);

		//Max size the pool can grow
		size_t m_max_size;

		//Data storage of our DATA and free list
		using DataStorage = union
		{
			typename std::aligned_storage<sizeof(DATA), alignof(DATA)>::type data;
			typename HANDLE::type_param next_free_slot;
		};

		//List of free slots
		typename HANDLE::type_param m_first_free_allocated;

		//Vector of the data associated to this pool
		std::vector<DataStorage> m_data;

	protected:
		typename HANDLE::type_param GetInternalIndex(const Accessor& handle) const
		{
			return handle.m_index;
		}
	};

	template<typename HANDLE, typename DATA>
	inline HandlePool<HANDLE, DATA>::~HandlePool()
	{
		if (m_data.size())
		{
			//Report leaks
			std::vector<bool> allocated;
			allocated.resize(m_data.size());

			for (auto& it : allocated) it = true;

			typename HANDLE::type_param free_index = m_first_free_allocated;
			while (free_index != HANDLE::kInvalid)
			{
				allocated[free_index] = false;
				free_index = GetNextFreeSlot(free_index);
			}

			//Report
			size_t num_allocated_handles = 0;
			for (size_t i = 0; i < allocated.size(); ++i)
			{
				//Check if it is allocated
				if (allocated[i])
				{
					//Destroy DATA
					(reinterpret_cast<DATA*>(&m_data[i]))->~DATA();

					num_allocated_handles++;
				}
			}
			if (num_allocated_handles > 0)
			{
				core::log("Pool still has some allocated handles, force deleted all handles\n");
			}
		}
	}

	//Init pool with a list of free slots avaliable
	template<typename HANDLE, typename DATA>
	inline void HandlePool<HANDLE, DATA>::Init(size_t max_size, size_t init_size)
	{
		assert(max_size < std::numeric_limits<typename HANDLE::type_param>::max() - 1);
		m_max_size = max_size;
		m_first_free_allocated = HANDLE::kInvalid;

		GrowDataStorage(init_size);
	}


	//Allocate a handle
	template<typename HANDLE, typename DATA>
	template<typename ...Args>
	inline HANDLE HandlePool<HANDLE, DATA>::Alloc(Args && ...args)
	{
		//If there is free slots, it will use the last one free
		if (m_first_free_allocated == HANDLE::kInvalid)
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
		if (m_first_free_allocated == HANDLE::kInvalid)
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