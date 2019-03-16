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
	private:
		size_t num_fences = 0;
	public:
		size_t num_workers = static_cast<size_t>(-1);
		bool force_hardware_affinity = true;
		size_t max_sleep = 1000;

		//Fences are reserved before the job system is created
		Fence ReserveFence();

		size_t GetNumFences() const
		{
			return num_fences;
		}
	};

	System* CreateSystem(const SystemDesc& system_desc);
	void DestroySystem(System* system);

	//Add job
	void AddJob(System* system, const JobFunction job, const void* data, const Fence fence);

	//Wait in fence
	void Wait(System* system, const Fence fence);
}

#endif //JOB_H_