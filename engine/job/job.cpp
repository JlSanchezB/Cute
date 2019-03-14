#include "job.h"
#include <vector>
#include <thread>

namespace
{
	struct Fence
	{

	};

	struct Worker
	{

	};

	struct System
	{
		//Fence vector
		std::vector<Fence> m_fences;
		//Worker vector
		std::vector<Worker> m_workers;

		//State

	};
}

namespace job
{
	System * CreateSystem(const SystemDesc & system_desc)
	{
		return nullptr;
	}

	void DestroySystem(System * system)
	{
	}

	Fence CreateFence(System * system)
	{
		return Fence();
	}

	void DestroyFence(System * system, Fence fence)
	{
	}

	void Start(System * system)
	{
	}

	void Stop(System * system)
	{
	}

	void AddJob(System * system, JobFunction job, const Fence fence)
	{
	}

	void Wait(System * system, const Fence fence)
	{
	}
}