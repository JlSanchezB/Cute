//////////////////////////////////////////////////////////////////////////
// Cute engine - Sync primitives
//////////////////////////////////////////////////////////////////////////

#ifndef SYNC_H_
#define SYNC_H_

#include <atomic>

namespace core
{
	class SpinLockMutex
	{
	public:
		void lock()
		{
			while (m_atomic.test_and_set(std::memory_order_acquire)) { ; }; //spin
		}

		void unlock()
		{
			m_atomic.clear(std::memory_order_release);
		}
	private:
		std::atomic_flag m_atomic = ATOMIC_FLAG_INIT;
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
