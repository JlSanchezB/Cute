#include "job.h"
#include <vector>
#include <array>
#include <cassert>
#include "job_queue.h"
#include <cstdlib>
#include <core/log.h>
#include <core/profile.h>
#include <core/sync.h>
#include <ext/imgui/imgui.h>

namespace
{
	bool g_thread_data_created = false;
	//Number of workers
	size_t g_num_workers = 1;

	//Each thread has it correct worker id using thread local storage variable
	thread_local size_t g_worker_id = 0;
}

namespace job
{
	void ThreadDataCreated()
	{
		g_thread_data_created = true;
	}

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

			wchar_t name_buffer[256];
			swprintf_s(name_buffer, L"Worker Thread %zd", m_worker_index);

			//Create a thread associated to this worker
			m_thread = std::make_unique<core::Thread>(name_buffer, core::ThreadPriority::Normal, &Worker::ThreadRun, this);
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
		std::unique_ptr<core::Thread> m_thread;
		//Running
		bool m_running = false;
		//Worker index
		size_t m_worker_index;
		//System
		System* m_system;
		//Queue
		Queue<Job, 4096> m_job_queue;
		//Count for yield, count of failed job search before to yield
		size_t m_count_for_yield = 0;

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

		bool m_single_thread_mode = false;

		size_t m_count_for_yield = 0;

		size_t m_begin_extra_workers = 0;

		//Stats
		std::atomic<size_t> m_jobs_added = 0;
		std::atomic<size_t> m_jobs_stolen = 0;

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
		if (g_thread_data_created)
		{
			core::LogError("ThreadData was created before the job system creation, that must never happen");
			return nullptr;
		}

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

		system->m_begin_extra_workers = num_workers;
		num_workers += system_desc.extra_workers;

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

		system->m_count_for_yield = system_desc.count_for_yield;

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

	void RenderImguiDebug(System* system, bool* activated)
	{
		if (ImGui::Begin("Job System", activated))
		{
			ImGui::Text("Num workers (%zu)", g_num_workers);
			ImGui::Separator();
			ImGui::Text("Num jobs added (%zu)", system->m_jobs_added.load());
			ImGui::Text("Num jobs stolen (%zu)", system->m_jobs_stolen.load());
			ImGui::Separator();
			bool single_frame_mode = job::GetSingleThreadMode(system);
			if (ImGui::Checkbox("Single thread mode", &single_frame_mode))
			{
				job::SetSingleThreadMode(system, single_frame_mode);
			}

			system->m_jobs_added = 0;
			system->m_jobs_stolen = 0;

			ImGui::End();
		}
	}

	void SetSingleThreadMode(System* system, bool single_thread_mode)
	{
		system->m_single_thread_mode = single_thread_mode;
	}

	bool GetSingleThreadMode(System * system)
	{
		return system->m_single_thread_mode;
	}

	void RegisterExtraWorker(System* system, size_t extra_worker_index)
	{
		g_worker_id = system->m_begin_extra_workers + extra_worker_index;
	}

	void AddJob(System * system, const JobFunction job, void* data, Fence& fence)
	{
		system->m_jobs_added++;

		if (system->m_single_thread_mode)
		{
			//Just run the job
			job(data);
		}
		else
		{
			//Increment the fence
			system->IncrementFence(fence);

			//Add job to current worker
			system->m_workers[g_worker_id]->AddJob(Job{ job, data, &fence });
		}
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
			m_system->m_jobs_stolen++;
			return true;
		}

		//Increase the count for yield of this worker
		m_count_for_yield++;

		if (m_count_for_yield >= m_system->m_count_for_yield)
		{
			m_count_for_yield = 0;
			//Yield thread
			std::this_thread::yield();
		}

		return false;
	}

	//Code running in the worker thread

	inline void Worker::ThreadRun()
	{
		//Set name to the profiler
		char name_buffer[256];
		sprintf_s(name_buffer, "Worker Thread %zd", m_worker_index);
		core::OnThreadCreate(name_buffer);

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