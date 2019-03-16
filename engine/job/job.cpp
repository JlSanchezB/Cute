#include "job.h"
#include <vector>
#include <cassert>
#include <thread>
#include <atomic>

namespace
{
	//Each thread has it correct worker id using thread local storage variable
	thread_local uint8_t g_worker_id = static_cast<uint8_t>(-1);
}

namespace job
{
	struct alignas(std::hardware_destructive_interference_size) FenceData
	{
		//Number of tasks waiting in this fence
		std::atomic_size_t value = 0;
	};

	struct alignas(std::hardware_destructive_interference_size) Worker
	{

	};

	struct System
	{
		//Fence vector
		std::unique_ptr<FenceData[]> m_fences;
		//Worker vector
		std::vector<Worker> m_workers;

		//State
		enum class State
		{
			Inited,
			Started,
			Stopping
		};

		State m_state;
	};

	System * CreateSystem(const SystemDesc & system_desc)
	{
		System* system = new System();

		//Config system
		system->m_fences = std::make_unique<FenceData[]>(system_desc.GetNumFences());

		//Main thread is the worker zero

		return system;
	}

	void DestroySystem(System * system)
	{
		//Stop job system if needed
		Stop(system);

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

	void AddJob(System * system, JobFunction job, const Fence fence)
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