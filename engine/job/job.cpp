#include "job.h"
#include <vector>
#include <array>
#include <cassert>
#include "job_queue.h"
#include <cstdlib>

namespace
{
	//Number of workers
	size_t g_num_workers = 1;

	//Each thread has it correct worker id using thread local storage variable
	thread_local size_t g_worker_id = static_cast<size_t>(-1);
}

namespace job
{
	//Get current worker index helper
	size_t GetWorkerIndex()
	{
		return g_worker_id;
	}

	//Get num workers helper
	size_t GetNumWorkers()
	{
		return g_num_workers;
	}

	//Job data
	struct Job
	{
		JobFunction function;
		void* data;
		Fence* fence;
	};

	//Worker
	class alignas(std::hardware_destructive_interference_size) Worker
	{
	public:
		Worker(size_t worker_index, bool main_thread, System* system) :
			m_worker_index(worker_index), m_system(system)
		{
			if (main_thread)
			{
				//Set local thread storage for fast access, inclusive for main thread
				g_worker_id = worker_index;
			}
		}

		void Start()
		{
			assert(m_worker_index > 0);

			m_running = true;

			//Create a thread associated to this worker
			m_thread = std::make_unique<std::thread>(&Worker::ThreadRun, this);
		}

		void Stop()
		{
			assert(m_worker_index > 0);

			m_running = false;

			if (m_thread)
			{
				//Wait until the current job is finished
				m_thread->join();

				//Delete thread
				m_thread.release();
			}
		}

		bool Steal(Job& job)
		{
			return m_job_queue.Steal(job);
		}

		void AddJob(const Job& job)
		{
			//Try to push the job
			while (!m_job_queue.Push(job))
			{
				//Yield thread
				std::this_thread::yield();
			};
		}

		bool GetJob(Job& job);

	private:
		//Thread if it needed
		std::unique_ptr<std::thread> m_thread;
		//Running
		bool m_running = false;
		//Worker index
		size_t m_worker_index;
		//System
		System* m_system;
		//Queue
		Queue<Job, 4096> m_job_queue;

		//Code running in the worker thread
		void ThreadRun();
	};

	struct System
	{
		//Worker vector
		std::vector<std::unique_ptr<Worker>> m_workers;

		//State
		enum class State
		{
			Stopped,
			Started,
			Stopping
		};

		State m_state = State::Stopped;

		bool StealJob(size_t current_worker_id, Job& job)
		{
			//Get random value
			size_t worker_to_steal = std::rand() % m_workers.size();

			if (worker_to_steal != current_worker_id)
			{
				return m_workers[worker_to_steal]->Steal(job);
			}
			else
			{
				return false;
			}

		}

		//Increment fence
		void IncrementFence(Fence& fence) const
		{
			fence.value.fetch_add(1);
		}

		//Decrement fence
		void DecrementFence(Fence& fence) const
		{
			fence.value.fetch_sub(1);
		}

		//Fence done
		bool IsFenceFinished(Fence& fence) const
		{
			return (fence.value == 0);
		}
	};

	System * CreateSystem(const SystemDesc & system_desc)
	{
		System* system = new System();

		//Init system
		size_t num_workers;
		if (system_desc.num_workers != static_cast<size_t>(-1))
		{
			num_workers = system_desc.num_workers;
		}
		else
		{
			//One worker per hardware thread
			num_workers = std::thread::hardware_concurrency();
		}
		system->m_workers.reserve(num_workers);
		
		g_num_workers = num_workers;

		//Main thread is the worker 0
		system->m_workers.push_back(std::make_unique<Worker>(0, true, system));

		//Create the rest of the workers
		for (size_t i = 1; i < num_workers; ++i)
		{
			system->m_workers.push_back(std::make_unique<Worker>(i, false, system));
		}

		//Start workers
		for (size_t i = 1; i < num_workers; ++i)
		{
			system->m_workers[i]->Start();
		}

		system->m_state = System::State::Started;

		return system;
	}

	void DestroySystem(System *& system)
	{
		//Only can be called in the main thread
		assert(g_worker_id == 0);

		system->m_state = System::State::Stopping;

		//Stop workers
		for (size_t i = 1; i < system->m_workers.size(); ++i)
		{
			system->m_workers[i]->Stop();
		}
		
		system->m_state = System::State::Stopped;

		//Destroy
		delete system;

		system = nullptr;
	}

	void AddJob(System * system, const JobFunction job, void* data, Fence& fence)
	{
		//Increment the fence
		system->IncrementFence(fence);

		//Add job to current worker
		system->m_workers[g_worker_id]->AddJob(Job{ job, data, &fence });
	}

	void Wait(System * system, Fence& fence)
	{
		auto& worker = *system->m_workers[g_worker_id].get();
		while (!system->IsFenceFinished(fence))
		{
			//Work for a job during waiting
			Job job;

			if (worker.GetJob(job))
			{
				//Execute
				job.function(job.data);

				//Decrement the fence
				system->DecrementFence(*job.fence);
			}
		}
	}
	
	//Worker inline functions
	inline bool Worker::GetJob(Job & job)
	{
		//Try to pop from the worker job queue
		if (m_job_queue.Pop(job))
		{
			return true;
		}

		//Try to steal from a random queue
		if (m_system->StealJob(m_worker_index, job))
		{
			return true;
		}

		//Yield thread
		std::this_thread::yield();

		return false;
	}

	//Code running in the worker thread

	inline void Worker::ThreadRun()
	{
		//Set local thread storage for fast access
		g_worker_id = m_worker_index;

		while (m_running)
		{
			//Get Job
			Job job;

			if (GetJob(job))
			{
				//Execute
				job.function(job.data);

				//Decrement the fence
				m_system->DecrementFence(*job.fence);
			}

		}
	}
}