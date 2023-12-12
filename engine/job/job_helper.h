//////////////////////////////////////////////////////////////////////////
// Cute engine - Job system helpers
//////////////////////////////////////////////////////////////////////////
#ifndef JOB_HELPER_H_
#define JOB_HELPER_H_

#include <vector>
#include <thread>
#include <core/virtual_buffer.h>

namespace job
{
	//Thread data can be only created after the job system is created
	void ThreadDataCreated();

	//Get current worker index
	size_t GetWorkerIndex();

	//Get num workers
	size_t GetNumWorkers();


	//ThreadData class creates a object from DATA for each worker
	//That allows us to access a DATA object per worker without sharing memory or syncing
	template<typename DATA>
	class ThreadData
	{
	public:
		ThreadData()
		{
			ThreadDataCreated();

			m_thread_data = std::make_unique<AlignedData<DATA>[]>(GetNumWorkers());
		}

		ThreadData(const ThreadData&) = delete;
		ThreadData& operator= (const ThreadData&) = delete;

		//Access current worker data
		//Used during working of the data
		DATA& Get()
		{
			return m_thread_data.get()[GetWorkerIndex()];
		}

		//Access any worker data
		//Used once the jobs are finish and collecting the data
		DATA& AccessThreadData(size_t worker_index)
		{
			return m_thread_data.get()[worker_index];
		}

		//Visit all data
		template<typename VISITOR>
		void Visit(VISITOR&& visitor)
		{
			const size_t size = GetNumWorkers();
			for (size_t i = 0; i < size; ++i)
			{
				visitor(m_thread_data.get()[i]);
			}
		}

	private:
		
		//Data needs to be aligned to cache lines, so we don't false shared between the workers
		template<typename DATA_TO_ALIGN>
		struct alignas(std::hardware_destructive_interference_size) AlignedData : DATA_TO_ALIGN
		{
		};

		//Vector of DATA for each worker
		std::unique_ptr<AlignedData<DATA>[]> m_thread_data;
	};

	template<size_t RESERVED_MEMORY>
	struct JobAllocationData
	{
		core::VirtualBufferInitied<RESERVED_MEMORY> buffer;
		size_t current_position = 0;
	};

	template<size_t RESERVED_MEMORY>
	class JobAllocator : public ThreadData<JobAllocationData<RESERVED_MEMORY>>
	{
	public:
		void Clear()
		{
			for (size_t i = 0; i < GetNumWorkers(); ++i)
			{
				ThreadData<JobAllocationData<RESERVED_MEMORY>>::AccessThreadData(i).current_position = 0;
			}
		}
		
		template<typename JOBDATA>
		JOBDATA* Alloc()
		{
			//static_assert(std::is_trivially_constructible<JOBDATA>::value);

			auto& buffer = ThreadData<JobAllocationData<RESERVED_MEMORY>>::Get().buffer;
			auto& position = ThreadData<JobAllocationData<RESERVED_MEMORY>>::Get().current_position;

			//Allocate sufficient space
			size_t alignment_offset = CalculateAlignment(alignof(JOBDATA), position);

			//Reserve memory as needed
			const size_t begin_offset = position + alignment_offset;
			buffer.SetCommitedSize(position + alignment_offset + sizeof(JOBDATA), false);
			void* data_ptr = reinterpret_cast<uint8_t*>(buffer.GetPtr()) + begin_offset;

			//Advance position
			position += (alignment_offset + sizeof(JOBDATA));

			//Return pointer
			return reinterpret_cast<JOBDATA*>(data_ptr);
		}
	private:
		inline size_t CalculateAlignment(size_t alignment, size_t offset)
		{
			size_t bias = offset % alignment;
			return (bias == 0) ? 0 : (alignment - bias);
		}
	};



}

#endif