//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system job helper
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_JOB_HELPER_H_
#define ENTITY_COMPONENT_JOB_HELPER_H_

#include <ecs/entity_component_system.h>
#include <job/job.h>
#include <job/job_helper.h>
#include <core/profile.h>

namespace ecs
{
	template<typename DATABASE_DECLARATION, typename FUNCTION, typename ...COMPONENTS>
	struct JobBucketData
	{
		//tuple for pointers of the components
		std::tuple<COMPONENTS*...> components;

		//function to call for each component
		FUNCTION kernel;

		//Instance interator
		InstanceIterator<DATABASE_DECLARATION> instance_iterator;

		//Begin instance
		size_t begin_instance;

		//End instance
		size_t end_instances;

		//Job for running this bucket
		static void Job(void* data)
		{
			MICROPROFILE_SCOPEI("Ecs", "Job", 0xFFFFAAAA);

			JobBucketData* this_bucket_data = reinterpret_cast<JobBucketData>(data);

			//Go for all the instances and call the kernel function
			for (InstanceIndexType instance_index = this_bucket_data->begin_instance; instance_index < (this_bucket_data->begin_instance + this_bucket_data->num_instances); ++instance_index)
			{
				this_bucket_data->instance_iterator.m_instance_index = instance_index;

				//Call kernel
				internal::caller_helper<DATABASE_DECLARATION>(kernel, data, this_bucket_data->instance_iterator, instance_index, std::make_index_sequence<sizeof...(COMPONENTS)>(), components);
			}
		}
	};

	//Add jobs for processing the kernel function
	//Jobs will be created using the job_allocator and sync to the fence
	//Kernel needs to be a function with parameters (job_data passed here, InstanceIterator, COMPONENTS)
	template<typename DATABASE_DECLARATION, typename ...COMPONENTS, typename FUNCTION, typename BITSET, typename JOB_ALLOCATOR>
	void AddJobs(job::System* job_system, job::Fence& fence, JOB_ALLOCATOR& job_allocator, size_t num_instances_per_job,
		FUNCTION&& kernel, void* job_data, BITSET&& zone_bitset)
	{
		//Calculate component mask
		const EntityTypeMask component_mask = EntityType<std::remove_const<COMPONENTS>::type...>::template EntityTypeMask<DATABASE_DECLARATION>();

		const ZoneType num_zones = internal::GetNumZones(DATABASE_DECLARATION::s_database);

		InstanceIterator<DATABASE_DECLARATION> instance_iterator;

		//Loop for all entity type that match the component mask
		core::visit<DATABASE_DECLARATION::EntityTypes::template Size()>([&](auto entity_type_index)
		{
			const EntityTypeType entity_type = static_cast<EntityTypeType>(entity_type_index.value);

			instance_iterator.m_entity_type = entity_type;

			using EntityTypeIt = typename DATABASE_DECLARATION::EntityTypes::template ElementType<entity_type_index.value>;
			if ((component_mask & EntityTypeIt::template EntityTypeMask<DATABASE_DECLARATION>()) == component_mask)
			{
				//Loop all zones in the bitmask
				for (ZoneType zone_index = 0; zone_index < num_zones; ++zone_index)
				{
					if (zone_bitset.test(zone_index))
					{
						instance_iterator.m_zone_index = zone_index;

						const InstanceIndexType num_instances = internal::GetNumInstances(DATABASE_DECLARATION::s_database, zone_index, entity_type);
						auto argument_component_buffers = std::make_tuple(
							internal::GetStorageComponentHelper<DATABASE_DECLARATION, COMPONENTS>(zone_index, entity_type)...);

						if (num_instances > 0)
						{
							size_t num_buckets = (num_instances / num_instances_per_job) + 1;

							for (size_t bucket_index = 0; bucket_index < num_buckets; ++num_buckets)
							{
								//Create job data
								using JobData = JobBucketData<DATABASE_DECLARATION, FUNCTION, COMPONENTS...>;
								
								JobData& job_data = *job_allocator.Alloc<JobData>();

								job_data.components = argument_component_buffers;
								job_data.begin_instance = bucket_index * num_instances_per_job;
								job_data.num_instances = std::min((bucket_index + 1) * num_instances_per_job);
								job_data.function = kernel;
								job_data.instance_iterator = instance_iterator;
								
								//Add job
								job::AddJob(job_system, JobData::Job, &job_data, fence);
							}
						}
					}
				}

			}
		});
	}
}

#endif //ENTITY_COMPONENT_JOB_HELPER_H_

