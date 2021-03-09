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
		size_t count_for_yield = 128;
	};

	System* CreateSystem(const SystemDesc& system_desc);
	void DestroySystem(System*& system);

	//Run all tasks in a single thread
	void SetSingleThreadMode(System* system, bool single_thread_mode);
	bool GetSingleThreadMode(System* system);

	//Add job
	void AddJob(System* system, const JobFunction job, void* data, Fence& fence);

	//Helper container for our lambda
	template<typename FUNCTION>
	struct Helper
	{
		//Function that knows how to call the function
		static void Job(void* data)
		{
			FUNCTION* function = reinterpret_cast<FUNCTION*>(data);
			(*function)();
		}
	};

	//Add job lambda
	template<typename FUNCTION, typename JOB_ALLOCATOR>
	void AddLambdaJob(System* system, const FUNCTION& job, JOB_ALLOCATOR& job_allocator, Fence& fence)
	{
		//Capture function in the job allocator
		FUNCTION* captured_function = new (job_allocator->Alloc<FUNCTION>()) FUNCTION(job);
		//Create a job with a specialized function that knows how to run that lambda
		AddJob(system, Helper<FUNCTION>::Job, captured_function, fence);
	}

	//Wait in fence
	void Wait(System* system, Fence& fence);
}

#endif //JOB_H_