//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a pool of generic handles
//////////////////////////////////////////////////////////////////////////

#ifndef HANDLE_POOL_H_
#define HANDLE_POOL_H_

#include "log.h"

#include <vector>
#include <algorithm>
#include <cassert>
#include <core/sync.h>

#ifdef _DEBUG
#define WEAKHANDLE_TRACKING
#endif

namespace core
{
	template <typename DATA, typename TYPE>
	class Handle;

	template<typename HANDLE>
	class HandlePool;

#ifdef WEAKHANDLE_TRACKING
	struct WeakHandleTracking
	{
		void AddWeakReference(size_t index)
		{
			core::MutexGuard guard(m_spin_lock_mutex);
			m_weak_handle_tracking[index]++;
		}

		void RemoveWeakReference(size_t index)
		{
			core::MutexGuard guard(m_spin_lock_mutex);
			m_weak_handle_tracking[index]--;
		}

		//Number of weak handles for each index
		std::vector<size_t> m_weak_handle_tracking;
		//Spin lock
		core::Mutex m_spin_lock_mutex;
	};
#endif

	template <typename DATA, typename TYPE>
	class HandleAccessor
	{
	protected:
		using type_param = TYPE;
		using data_param = DATA;

		//Index inside the handlepool to access the data associated to this handle 
		TYPE m_index;

		//Invalid handle
		static const TYPE kInvalid = -1;

		template<typename HANDLE>
		friend class HandlePool;

		HandleAccessor() : m_index(kInvalid)
		{
		}
	

	public:
		bool IsValid() const
		{
			return m_index != kInvalid;
		}

		bool operator==(const HandleAccessor& b)
		{
			return m_index == b.m_index;
		}

		bool operator!=(const HandleAccessor& b)
		{
			return m_index != b.m_index;
		}
	};

	//Weak handle only can be create from a handle
	//They can be copied
	template <typename DATA, typename TYPE>
	class WeakHandle : public HandleAccessor<DATA, TYPE>
	{
		using Accessor = HandleAccessor<DATA, TYPE>;

#ifdef WEAKHANDLE_TRACKING
		//Access to the pool associated to this handle
		inline static WeakHandleTracking* s_tracking_pool = nullptr;

		template<typename HANDLE>
		friend class HandlePool;
#endif

	public:
		//Default constructor
		WeakHandle()
		{
		}

#ifdef WEAKHANDLE_TRACKING
		WeakHandle(const Handle<DATA, TYPE>& a);

		WeakHandle(const WeakHandle & a)
		{
			if (Accessor::m_index != Accessor::kInvalid)
			{
				s_tracking_pool->RemoveWeakReference(Accessor::m_index);
			}

			Accessor::m_index = a.m_index;

			if (Accessor::m_index != Accessor::kInvalid)
			{
				s_tracking_pool->AddWeakReference(Accessor::m_index);
			}
		}

		WeakHandle& operator=(const WeakHandle& a)
		{
			if (Accessor::m_index != Accessor::kInvalid)
			{
				s_tracking_pool->RemoveWeakReference(Accessor::m_index);
			}

			Accessor::m_index = a.m_index;

			if (Accessor::m_index != Accessor::kInvalid)
			{
				s_tracking_pool->AddWeakReference(Accessor::m_index);
			}

			return *this;
		}

		WeakHandle(WeakHandle&& a)
		{
			Accessor::m_index = a.m_index;
			a.m_index = Accessor::kInvalid;
		}

		WeakHandle& operator=(WeakHandle&& a)
		{
			Accessor::m_index = a.m_index;
			a.m_index = Accessor::kInvalid;

			return *this;
		}

		~WeakHandle()
		{
			if (Accessor::m_index != Accessor::kInvalid)
			{
				s_tracking_pool->RemoveWeakReference(Accessor::m_index);
			}
		}
#endif
	};

	//Handles can only be created from a pool and they can not be copied, only moved
	template <typename DATA, typename TYPE>
	class Handle : public WeakHandle<DATA, TYPE>
	{
		using Accessor = HandleAccessor<DATA, TYPE>;

		//Private constructor of a handle, only a pool can create valid handles
		Handle(TYPE index)
		{
			//The destination handle needs to be invalid
			assert(Accessor::m_index == Accessor::kInvalid);
			Accessor::m_index = index;
		}

		template<typename HANDLE>
		friend class HandlePool;

		template<typename DATA, typename TYPE>
		friend class WeakHandle;

	public:
		using WeakHandleVersion = WeakHandle<DATA, TYPE>;

		//Public constructor
		Handle()
		{
		}

		//Disable copy construction and copy assing, only is allowed to move

		Handle(const Handle& a) = delete;

		Handle(Handle&& a)
		{
			//The destination handle needs to be invalid
			assert(Accessor::m_index == Accessor::kInvalid);
			Accessor::m_index = a.m_index;
			a.m_index = Accessor::kInvalid;
		}

		Handle& operator=(const Handle& a) = delete;

		Handle& operator=(Handle&& a)
		{
			//The destination handle needs to be invalid
			assert(Accessor::m_index == Accessor::kInvalid);
			Accessor::m_index = a.m_index;
			a.m_index = Accessor::kInvalid;
			return *this;
		}

		~Handle()
		{
			//Only invalid index can be destructed, if not we will have leaks
			assert(Accessor::m_index == Accessor::kInvalid);
		}
	};

	template<typename DATA, typename TYPE>
	inline WeakHandle<DATA, TYPE> AsWeak(const Handle<DATA, TYPE>& handle)
	{
		return WeakHandle<DATA, TYPE>(handle);
	}


	//Pool of resources
	template<typename HANDLE>
	class HandlePool
#ifdef WEAKHANDLE_TRACKING
		: public WeakHandleTracking
#endif
	{
		using DATA = typename HANDLE::data_param;

		static_assert(std::is_default_constructible<DATA>::value);
	protected:
		using Accessor = HandleAccessor<typename HANDLE::data_param, typename HANDLE::type_param>;

	public:
		using HandleType = HANDLE;

#ifdef WEAKHANDLE_TRACKING
		HandlePool()
		{
			//Init tracking pool
			WeakHandle<typename HANDLE::data_param, typename HANDLE::type_param>::s_tracking_pool = this;
		}
#endif
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
			assert(handle.IsValid());
			return *reinterpret_cast<DATA*>(&m_data[handle.m_index].data);
		}

		const DATA& operator[](const Accessor& handle) const
		{
			assert(handle.IsValid());
			return *reinterpret_cast<const DATA*>(&m_data[handle.m_index].data);
		}

		size_t Size() const
		{
			return m_size;
		}

		size_t MaxSize() const
		{
			return m_max_size;
		}

	private:
		typename HANDLE::type_param& GetNextFreeSlot(const typename HANDLE::type_param& index)
		{
			return m_data[index].next_free_slot;
		}

		void GrowDataStorage(size_t new_size, bool init = false);

		//Max size the pool can grow
		size_t m_max_size = 0;

		//Current size
		size_t m_size = 0;;

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

	template<typename HANDLE>
	inline HandlePool<HANDLE>::~HandlePool()
	{
		if (m_data.size() && Size() > 0)
		{
			//Report leaks
			std::vector<uint8_t> allocated;
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
				core::LogWarning("Pool still has some allocated handles <%i>, force deleted all handles", num_allocated_handles);
			}
		}
	}

	//Init pool with a list of free slots avaliable
	template<typename HANDLE>
	inline void HandlePool<HANDLE>::Init(size_t max_size, size_t init_size)
	{
		assert(max_size <= std::numeric_limits<typename HANDLE::type_param>::max() - 1);
		assert(init_size <= max_size);
		m_max_size = max_size;
		m_first_free_allocated = HANDLE::kInvalid;
		m_size = 0;

		GrowDataStorage(init_size, true);		
	}


	//Allocate a handle
	template<typename HANDLE>
	template<typename ...Args>
	inline HANDLE HandlePool<HANDLE>::Alloc(Args && ...args)
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
				throw std::runtime_error::exception("Out of handles");
				return HANDLE(HANDLE::kInvalid);
			}
		}

		//Our handle will be allocated in m_first_free_allocated
		auto handle_slot = m_first_free_allocated;
		//Get next free slot
		m_first_free_allocated = GetNextFreeSlot(m_first_free_allocated);

		//Create DATA
		new(&m_data[handle_slot]) DATA(std::forward<Args>(args)...);

		m_size++;

#ifdef WEAKHANDLE_TRACKING
		//Init tracking of weak handles
		m_weak_handle_tracking[handle_slot] = 0;
#endif
		
		return HANDLE(handle_slot);
	}

	//Free unused handle
	template<typename HANDLE>
	inline void HandlePool<HANDLE>::Free(HANDLE & handle)
	{
		if (handle.IsValid())
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

			m_size--;

#ifdef WEAKHANDLE_TRACKING
			//Add log if there is still a weak handle
			if (m_weak_handle_tracking[handle.m_index] > 0)
			{
				core::LogWarning("Handle <%i> has been deleted, but still there weakhandles from it", handle.m_index);
			}
#endif

			//Reset handle to an invalid, it will avoid it keep the index
			handle.m_index = HANDLE::kInvalid;
		}
	}

	template<typename HANDLE>
	inline void HandlePool<HANDLE>::GrowDataStorage(size_t new_size, bool init)
	{
		//assert(m_first_free_allocated == 1);

		//Old size
		size_t old_size = m_data.size();

		if (init)
		{
			//Reserve the data
			m_data.resize(new_size);
		}
		else
		{
			//We need to move all the objects from the old data to the new
			//The vector is just an storage container
			std::vector<DataStorage> new_data;
			new_data.resize(new_size);

			for (size_t i = 0; i < old_size; ++i)
			{
				new(&new_data[i]) DATA(std::move(*reinterpret_cast<DATA*>(&m_data[i])));
			}
			//Move the array
			m_data = std::move(new_data);
		}

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

#ifdef WEAKHANDLE_TRACKING
		{
			core::MutexGuard guard(m_spin_lock_mutex);
			m_weak_handle_tracking.resize(new_size);
		}
#endif
	}

#ifdef WEAKHANDLE_TRACKING
	template<typename DATA, typename TYPE>
	inline WeakHandle<DATA, TYPE>::WeakHandle(const Handle<DATA, TYPE>& a)
	{
		Accessor::m_index = a.m_index;

		if (Accessor::m_index != Accessor::kInvalid)
		{
			s_tracking_pool->AddWeakReference(Accessor::m_index);
		}
	}
#endif
}

#endif //HANDLE_POOL_H_