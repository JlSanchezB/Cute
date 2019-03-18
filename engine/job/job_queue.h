//////////////////////////////////////////////////////////////////////////
// Cute engine - Job system queue
//////////////////////////////////////////////////////////////////////////
#ifndef JOB_QUEUE_H_
#define JOB_QUEUE_H_

#include <array>
#include <atomic>

namespace job
{
	//JobQueue
	//Lock free queue
	//Push and Pop is LIFO (only can be done from the worker thread, cache lines are hot around end)
	//Steal is FIFO, used from other threads (cache lines are hot in the begin, so no sharing between push/pop and steal)
	template<typename JOB, size_t NUM_JOBS>
	class Queue
	{
	public:
		//Push a new job in the end
		//Returns false if the job could not be added (queue full)
		bool Push(const JOB& job)
		{
			size_t begin = m_begin_index.load(std::memory_order::memory_order_acquire);
			size_t end = m_end_index.load(std::memory_order::memory_order_acquire);

			//Maybe begin moves left here, but no problem, next time will be ready to push
			if (PreviousIndex(begin) != NextIndex(end))
			{
				m_jobs[end] = job;

				end = NextIndex(end);

				//Store end
				m_end_index.store(end, std::memory_order::memory_order_release);

				return true;
			}
			else
			{
				//Full
				return false;
			}
		}

		//Pop a job if there is any job
		//Only use from the worker, pop from end
		bool Pop(JOB& job)
		{
			//Reserve a slot at the end (we pop decrementing the end index)
			//Maybe the ring buffer is empty, but we can handle that after
			//At the moment we want to reserve the slot as fast as possible
			size_t end = m_end_index.load(std::memory_order::memory_order_acquire);
			end = PreviousIndex(end);
			m_end_index.store(end, std::memory_order::memory_order_release);

			//Read the top index, maybe a steal had happen
			size_t begin = m_begin_index.load(std::memory_order::memory_order_acquire);

			if (begin == NextIndex(end)) //end is decremented
			{
				//The ring buffer was full anyway, so really nothing to do

				//Restore the end buffer
				m_end_index.store(NextIndex(end), std::memory_order::memory_order_release);

				//Nothing to pop
				return false;
			}
			else if (begin == end) //end is decremented
			{
				//Begin has incremented and now match the decremented end
				//This can only happens when a steal has happen, so we need to cancel our pop

				//We restore the end index
				m_end_index.store(NextIndex(end), std::memory_order::memory_order_release);

				//The ring buffer has not overflowed because the push stops when (PreviousIndex(begin) != NextIndex(end))

				//Nothing to pop
				return false;
			}
			else if (NextIndex(begin) == end)
			{
				job = m_jobs[begin];

				//Restore the end to the begin as we predecremented the end index
				//The ring is empty, or we will increment the begin index or the steal worker will do
				m_end_index.store(NextIndex(begin), std::memory_order::memory_order_release);

				//Looks safe but maybe the steal happens now, we only have 1 job left.
				//In this case we are going to behave exactly like a steal, using the begin index as sync
				if (m_begin_index.compare_exchange_strong(begin, NextIndex(begin), std::memory_order_acq_rel))
				{
					//We manage to increment without issues
					return true;
				}
				else
				{
					//A steal has happen
					return false;
				}
			}
			else
			{
				//It was more than 1 job between pop and steal
				//It is save to just capture the job
				//That is the most common case and we don't needed a lot of sync

				job = m_jobs[end];

				return true;
			}

		}

		//Steal from another worker if there is any job
		//Steal is from the begin
		bool Steal(JOB& job)
		{
			size_t begin = m_begin_index.load(std::memory_order::memory_order_acquire);
			size_t end = m_end_index.load(std::memory_order::memory_order_acquire);

			if (begin != end)
			{
				//There is something to steal, maybe
				job = m_jobs[begin];

				//Trying to confirm that this job can be steal
				if (m_begin_index.compare_exchange_strong(begin, NextIndex(begin), std::memory_order_acq_rel))
				{
					//We manage to steal this index
					return true;
				}
				else
				{
					//Other thread steal this index, nothing to steal
					return false;
				}
			}
			else
			{
				//Nothing in the queue
				return false;
			}
		}

	private:

		static size_t NextIndex(size_t index)
		{
			return (index + 1) % NUM_JOBS;
		}

		static size_t PreviousIndex(size_t index)
		{
			return (index - 1 + NUM_JOBS) % NUM_JOBS;
		}

		//Begin index of the queue (steal)
		std::atomic_size_t m_begin_index = 0;

		//Array of jobs
		std::array<JOB, NUM_JOBS> m_jobs;

		//End index of the queue (push and pop)
		std::atomic_size_t m_end_index = 0;
	};
}

#endif //JOB_QUEUE_H_