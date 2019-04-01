//////////////////////////////////////////////////////////////////////////
// Cute engine - Job system
//////////////////////////////////////////////////////////////////////////
#ifndef JOB_H_
#define JOB_H_

#include <atomic>
#include <thread>

namespace job
{
	struct System;
	
	//Fence (each system can be declared, recomended a global variable)
	class alignas(std::hardware_destructive_interference_size) Fence
	{
		friend struct System;

		//Number of tasks waiting in this fence
		std::atomic_size_t value = 0;
	};

	using JobFunction = void(*)(void*);

	struct SystemDesc
	{
		size_t num_workers = static_cast<size_t>(-1);
		bool force_hardware_affinity = true;
		size_t max_sleep = 1000;
	};

	System* CreateSystem(const SystemDesc& system_desc);
	void DestroySystem(System*& system);

	//Run all tasks in a single thread
	void SetSingleThreadMode(System* system, bool single_thread_mode);
	bool GetSingleThreadMode(System* system);

	//Add job
	void AddJob(System* system, const JobFunction job, void* data, Fence& fence);

	//Wait in fence
	void Wait(System* system, Fence& fence);
}

#endif //JOB_H_