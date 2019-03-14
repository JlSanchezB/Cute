//////////////////////////////////////////////////////////////////////////
// Cute engine - Job system
//////////////////////////////////////////////////////////////////////////
#ifndef JOB_H_
#define JOB_H_

namespace job
{
	struct System;
	using Fence = size_t;

	using JobFunction = void(*)(void*);

	struct SystemDesc
	{
		size_t num_workers = static_cast<size_t>(-1);
		bool force_hardware_affinity = true;
		size_t max_sleep = 1000;
	};

	System* CreateSystem(const SystemDesc& system_desc);
	void DestroySystem(System* system);

	//Fences can only be created before starting the job system
	Fence CreateFence(System* system);
	void DestroyFence(System* system, Fence fence);

	//Start job system, create workers and be ready for jobs
	void Start(System* system);
	//Stop all workers
	void Stop(System* system);

	//Add job
	void AddJob(System* system, JobFunction job, const Fence fence);

	//Wait in fence
	void Wait(System* system, const Fence fence);
}

#endif //JOB_H_