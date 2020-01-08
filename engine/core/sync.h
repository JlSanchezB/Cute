//////////////////////////////////////////////////////////////////////////
// Cute engine - Sync primitives
//////////////////////////////////////////////////////////////////////////

#ifndef SYNC_H_
#define SYNC_H_

#include <atomic>
#include <thread>
#include <immintrin.h>

namespace core
{
	class SpinLockMutex
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
	};

	class SpinLockMutexGuard
	{
	public:
		SpinLockMutexGuard(SpinLockMutex& spin_lock) : m_spin_lock(spin_lock)
		{
			m_spin_lock.lock();
		}
		~SpinLockMutexGuard()
		{
			m_spin_lock.unlock();
		}

	private:
		SpinLockMutex& m_spin_lock;
	};
}

#endif //SYNC_H_
