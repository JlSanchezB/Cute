#include "job.h"
#include <vector>
#include <array>
#include <cassert>
#include <thread>
#include <atomic>
#include "job_queue.h"

namespace
{
	//Each thread has it correct worker id using thread local storage variable
	thread_local size_t g_worker_id = static_cast<size_t>(-1);
}

namespace job
{
	//Job data
	struct Job
	{
		JobFunction function;
		void* data;
		Fence fence;
	};

	//Fence
	struct alignas(std::hardware_destructive_interference_size) FenceData
	{
		//Number of tasks waiting in this fence
		std::atomic_size_t value = 0;
	};


	//Worker
	class alignas(std::hardware_destructive_interference_size) Worker
	{
	public:
		Worker(size_t worker_index, bool main_thread, System* system) :
			m_worker_index(worker_index), m_system(system)
		{
			if (!main_thread)
			{
				//Create a thread associated to this worker
				m_thread = std::make_unique<std::thread>(&Worker::ThreadRun, this);
			}
			else
			{
				//Set local thread storage for fast access, inclusive for main thread
				g_worker_id = worker_index;
			}
		}

		void Start()
		{

		}
		void Stop()
		{

		}
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
		void ThreadRun()
		{
			//Set local thread storage for fast access
			g_worker_id = m_worker_index;

			while (m_running)
			{
				//Get Job

				//Execute
			}
		}
	};

	struct System
	{
		//Fence vector
		std::unique_ptr<FenceData[]> m_fences;
		//Worker vector
		std::vector<Worker*> m_workers;

		//Num fences
		size_t m_num_fences;
		//State
		enum class State
		{
			Stopped,
			Started,
			Stopping
		};

		State m_state = State::Stopped;
	};

	System * CreateSystem(const SystemDesc & system_desc)
	{
		System* system = new System();

		//Init system
		system->m_num_fences = system_desc.GetNumFences();
		system->m_fences = std::make_unique<FenceData[]>(system->m_num_fences);

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
		
		//Main thread is the worker 0
		system->m_workers.push_back(new Worker(0, true, system));

		//Create the rest of the workers
		for (size_t i = 1; i < num_workers; ++i)
		{
			system->m_workers.push_back(new Worker(i, false, system));
		}

		//Start system
		//Start(system);

		return system;
	}

	void DestroySystem(System * system)
	{
		//Only can be called in the main thread

		//Stop job system if needed
		//Stop(system);

		//Destroy
		delete system;
	}

	void Start(System * system)
	{
		//Only called from the main thread

		//Start all the workers

	}

	void Stop(System * system)
	{
		//Only called from the main thread

		//Wait until all the fences are finished
		
		//Stop all workers
	}

	void AddJob(System * system, const JobFunction job, const void* data, const Fence fence)
	{
	}

	void Wait(System * system, const Fence fence)
	{
	}
	Fence SystemDesc::ReserveFence()
	{
		return num_fences++;
	}
}