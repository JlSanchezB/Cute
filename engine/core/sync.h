﻿//////////////////////////////////////////////////////////////////////////
// Cute engine - Sync primitives
//////////////////////////////////////////////////////////////////////////

#ifndef SYNC_H_
#define SYNC_H_

#include <atomic>
#include <thread>
#include <immintrin.h>
#include <mutex>

namespace core
{
	//Keep a specific mutex class for testing differences between different mutex implementations

	using Mutex = std::mutex;
	/*class Mutex
	{
	public:
		void lock()
		{
			for (int spin_count = 0; !try_lock(); ++spin_count)
			{
				if (spin_count < 16)
					_mm_pause();
				else
				{
					std::this_thread::yield();
					spin_count = 0;
				}
			}
		}

		bool try_lock()
		{
			return !locked.load(std::memory_order_relaxed) && !locked.exchange(true, std::memory_order_acquire);
		}	

		void unlock()
		{
			locked.store(false, std::memory_order_release);
		}
	private:
		std::atomic<bool> locked{ false };
	};*/

	class MutexGuard
	{
	public:
		MutexGuard(Mutex& spin_lock) : m_spin_lock(spin_lock)
		{
			m_spin_lock.lock();
		}
		~MutexGuard()
		{
			m_spin_lock.unlock();
		}

	private:
		Mutex& m_spin_lock;
	};

	enum class ThreadPriority
	{
		Normal,
		Background
	};

	class Thread : public std::thread
	{
	public:
		template<class FUNCTION, class ...ARGS>
		Thread(const wchar_t* name, ThreadPriority thread_priority, FUNCTION&& f, ARGS&&... args) : std::thread(std::forward<FUNCTION>(f), std::forward<ARGS>(args)...)
		{
			//Init platform dependent part
			Init(name, thread_priority);
		}
	private:
		//Platform dependent init
		void Init(const wchar_t* name, ThreadPriority thread_priority);
	};
}

#endif //SYNC_H_
