//////////////////////////////////////////////////////////////////////////
// Cute engine - Entity component system job helper
//////////////////////////////////////////////////////////////////////////
#ifndef ENTITY_COMPONENT_JOB_HELPER_H_
#define ENTITY_COMPONENT_JOB_HELPER_H_

#include <ecs/entity_component_system.h>
#include <job/job.h>
#include <job/job_helper.h>
#include <core/profile.h>

PROFILE_DEFINE_MARKER(g_profile_marker_ECSJob, "ECSJob", "ECS", 0xFFFFAAAA);

namespace ecs
{
	template<typename DATABASE_DECLARATION, typename FUNCTION, typename JOB_DATA, typename ...COMPONENTS>
	struct JobBucketData
	{
		//tuple for pointers of the components
		std::tuple<COMPONENTS*...> components;

		//function to call for each component
		void(*kernel)(JOB_DATA*, const InstanceIterator<DATABASE_DECLARATION>&, COMPONENTS&...);

		//Job data
		JOB_DATA* job_data;

		//Instance interator
		InstanceIterator<DATABASE_DECLARATION> instance_iterator;

		//Begin instance
		InstanceIndexType begin_instance;

		//End instance
		InstanceIndexType end_instance;

		//Microprofile token
		core::ProfileMarker* microprofile_token = nullptr;

		//Job for running this bucket
		static void Job(void* bucket_job_data)
		{
			JobBucketData* this_bucket_job_data = reinterpret_cast<JobBucketData*>(bucket_job_data);

			PROFILE_SCOPE_MARKER((this_bucket_job_data->microprofile_token) ? *this_bucket_job_data->microprofile_token : g_profile_marker_ECSJob);

			//Go for all the instances and call the kernel function
			for (InstanceIndexType instance_index = this_bucket_job_data->begin_instance; instance_index < this_bucket_job_data->end_instance; ++instance_index)
			{
				this_bucket_job_data->instance_iterator.m_instance_index = instance_index;

				//Call kernel
				internal::caller_helper<DATABASE_DECLARATION>(this_bucket_job_data->kernel, this_bucket_job_data->job_data, this_bucket_job_data->instance_iterator, instance_index, std::make_index_sequence<sizeof...(COMPONENTS)>(), this_bucket_job_data->components);
			}
		}
	};

	//Add jobs for processing the kernel function
	//Jobs will be created using the job_allocator and sync to the fence
	//Kernel needs to be a function with parameters (job_data passed here, InstanceIterator, COMPONENTS)
	template<typename DATABASE_DECLARATION, typename ...COMPONENTS, typename FUNCTION, typename BITSET, typename JOB_ALLOCATOR, typename JOB_DATA>
	void AddJobs(job::System* job_system, job::Fence& fence, JOB_ALLOCATOR& job_allocator, size_t num_instances_per_job,
		FUNCTION&& kernel, JOB_DATA* job_data, BITSET&& zone_bitset, core::ProfileMarker* profile_token = nullptr)
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
							const InstanceIndexType num_buckets = (num_instances / static_cast<InstanceIndexType>(num_instances_per_job)) + 1;

							for (InstanceIndexType bucket_index = 0; bucket_index < num_buckets; ++bucket_index)
							{
								//Create job data
								using JobBucketDataT = JobBucketData<DATABASE_DECLARATION, FUNCTION, JOB_DATA, COMPONENTS...>;
								
								JobBucketDataT* job_bucket_data = job_allocator->Alloc<JobBucketDataT>();

								job_bucket_data->components = argument_component_buffers;
								job_bucket_data->begin_instance = bucket_index * static_cast<InstanceIndexType>(num_instances_per_job);
								job_bucket_data->end_instance = std::min((bucket_index + 1) * static_cast<InstanceIndexType>(num_instances_per_job), num_instances);
								job_bucket_data->kernel = kernel;
								job_bucket_data->instance_iterator = instance_iterator;
								job_bucket_data->job_data = job_data;
								job_bucket_data->microprofile_token = profile_token;
								
								//Add job
								job::AddJob(job_system, JobBucketDataT::Job, job_bucket_data, fence);
							}
						}
					}
				}

			}
		});
	}
}

#endif //ENTITY_COMPONENT_JOB_HELPER_H_

