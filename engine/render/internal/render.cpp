#include <render/render.h>
#include <ext/tinyxml2/tinyxml2.h>
#include <core/log.h>
#include <render/render_helper.h>
#include "render_system.h"
#include <render/render_resource.h>
#include <core/profile.h>
#include "render_pass.h"

#include <cassert>
#include <stdarg.h>
#include <utility>

namespace
{
	template<class CONTAINER>
	void DestroyResources(display::Device* device, CONTAINER& container)
	{
		for (auto& [key, item] : container)
		{
			if (item.resource)
			{
				item.resource->Destroy(device);
			}
		}

		container.clear();
	}

	template<class CONTAINER>
	void DestroyPasses(display::Device* device, CONTAINER& container)
	{
		for (auto& [key, item] : container)
		{
			item->Destroy(device);
		}

		container.clear();
	}

	constexpr uint32_t kRenderProfileColour = 0xFF3333FF;

	//Sync fence, it avoids the render frame been used before the render has been submited
	job::Fence g_render_fence;
}

namespace render
{
	Resource * RenderContext::GetResource(const ResourceName& name) const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);

		//Then check system resources
		return render::GetResource(render_context->m_render_pass_system, name);
	}

	bool RenderContext::AddPassResource(const ResourceName& name, std::unique_ptr<Resource>&& resource)
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);

		return render_context->m_render_pass_system->AddResource(name, resource, ResourceSource::Pass);
	}

	Frame & RenderContext::GetRenderFrame()
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_render_pass_system->m_frame_data;
	}

	Pass * RenderContext::GetRootPass() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_root_pass;
	}

	display::Device * RenderContext::GetDevice() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_display_device;
	}

	display::Context * RenderContext::GetContext() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_display_context;
	}

	const PassInfo& RenderContext::GetPassInfo() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_pass_info;
	}

	void RenderContext::SetContext(display::Context * context)
	{
		auto render_context = reinterpret_cast<RenderContextInternal*>(this);
		render_context->m_display_context = context;
	}

	void RenderContext::UpdatePassInfo(const PassInfo & pass_info)
	{
		auto render_context = reinterpret_cast<RenderContextInternal*>(this);
		render_context->m_pass_info = pass_info;
	}

	ResourceName System::LoadResource(LoadContext& load_context, const char* prefix)
	{
		//Get type and name
		const char* resource_type_string = load_context.current_xml_element->Attribute("type");
		const char* resource_name_string = load_context.current_xml_element->Attribute("name");
		std::string prefix_name_string;
		if (prefix)
		{
			prefix_name_string = std::string(prefix) + resource_name_string;
			resource_name_string = prefix_name_string.c_str();
		}

		RenderClassType resource_type(resource_type_string);
		ResourceName resource_name(resource_name_string);
		if (resource_type_string && resource_name_string)
		{
			auto& resource_factory_it = m_resource_factories_map[resource_type];
			if (resource_factory_it)
			{
				auto& resources_it = m_resources_map[resource_name];
				if (!resources_it)
				{
					auto& factory = *resource_factory_it;

					assert(factory.get());

					//Create resource container
					auto resource_instance = factory->Create();

					assert(resource_instance);
					assert(resource_instance->Type() == resource_type);

					load_context.name = resource_name_string;

					//Load resource
					resource_instance->Load(load_context);

					core::LogInfo("Created Resource <%s> type <%s>", resource_name_string, resource_type_string);

					AddResource(resource_name, std::unique_ptr<render::Resource>(resource_instance), ResourceSource::PassDescriptor);
					
					return resource_name;

				}
				else
				{
					AddError(load_context, "Resource name <%s> has been already added", resource_name_string);
				}
			}
			else
			{
				AddError(load_context, "Resource type <%s> is not register", resource_type_string);
			}
		}
		else
		{
			AddError(load_context, "Resource has not attribute type or name");
		}
		return ResourceName();
	}

	Pass* System::LoadPass(LoadContext& load_context)
	{
		//Create the pass
		const char* pass_type = load_context.current_xml_element->Name();
		const char* pass_name = load_context.current_xml_element->Attribute("name");

		
		auto& pass_factory_it = m_pass_factories_map[RenderClassType(pass_type)];
		if (pass_factory_it)
		{
			//Load the pass
			auto& factory = *pass_factory_it;

			assert(factory.get());

			//Create pass instance
			auto pass_instance = factory->Create();

			assert(pass_instance);
			assert(pass_instance->Type() == RenderClassType(pass_type));

			load_context.name = pass_type;

			//Load pass
			pass_instance->Load(load_context);

			return pass_instance;
		}
		else
		{
			AddError(load_context, "Pass type <%s> is not register", pass_type);
			return nullptr;
		}
	}

	RenderContextInternal * System::CreateRenderContext(display::Device * device, const PassName & pass, const PassInfo & pass_info, std::vector<std::string>& errors)
	{
		//Get pass
		auto render_pass = GetPass(this, pass);
		if (render_pass)
		{
			//Create Render Context
			RenderContextInternal* render_context = m_render_context_pool.Alloc(this, device, pass_info, render_pass);

			ErrorContext errors_context;

			//Allow the passes to init the render context 
			render_pass->InitPass(*render_context, device, errors_context);

			errors = std::move(errors_context.errors);

			if (errors.empty())
			{
				core::LogInfo("Created a render pass <%s> from definition pass", pass.GetValue());
				return render_context;
			}
			else
			{
				core::LogError("Errors creating a render pass <%s> from definition pass", pass.GetValue());
				for (auto& error : errors)
				{
					core::LogError(error.c_str());
				}

				DestroyRenderContext(render_context);
				return nullptr;
			}

		}
		else
		{
			errors.push_back(std::string("Pass not found"));
			core::LogError("Errors creating a render pass <%s>, definition pass doesn't exist", pass.GetValue());
			return nullptr;
		}
	}

	void System::DestroyRenderContext(RenderContextInternal *& render_context)
	{
		//Destroy context resources
		m_render_context_pool.Free(render_context);

		render_context = nullptr;
	}

	bool System::Load(LoadContext& load_context, const char* descriptor_file_buffer, size_t descriptor_file_buffer_size)
	{
		tinyxml2::XMLDocument xml_doc;

		tinyxml2::XMLError result = xml_doc.Parse(descriptor_file_buffer, descriptor_file_buffer_size);

		if (result != tinyxml2::XML_SUCCESS)
		{
			AddError(load_context, "Error parsing the descriptor file");
			return false;
		}

		tinyxml2::XMLNode* root = xml_doc.FirstChildElement("Root");
		if (root == nullptr)
		{
			AddError(load_context, "Root node doesn't exist");
			return false;
		}

		//Set the xml doc to the load context
		load_context.xml_doc = &xml_doc;

		//Load global resources
		tinyxml2::XMLElement* global = root->FirstChildElement("Global");
		if (global)
		{
			tinyxml2::XMLElement* resource = global->FirstChildElement();
			while (resource)
			{
				if (strcmp(resource->Name(), "Resource") == 0)
				{
					//It is a resource
					load_context.current_xml_element = resource;

					LoadResource(load_context);
				}
				else
				{
					AddError(load_context, "Global element <%s> not supported", resource->Name());
				}

				resource = resource->NextSiblingElement();
			}
		}

		//Load Passes
		tinyxml2::XMLElement* passes_element = root->FirstChildElement("Passes");
		if (passes_element)
		{
			tinyxml2::XMLElement* pass_element = passes_element->FirstChildElement();
			while (pass_element)
			{
				if (CheckNodeName(pass_element, "Pass"))
				{
					const char* pass_name_string = pass_element->Attribute("name");
					PassName pass_name(pass_name_string);
					if (pass_name_string)
					{
						auto& pass_it = m_passes_map[pass_name];
						if (!pass_it)
						{
							load_context.current_xml_element = pass_element;
							load_context.name = pass_name_string;
							load_context.pass_name = pass_name_string;

							//It is a root pass (usually a context pass), must have name so the game can find it

							Pass* pass = LoadPass(load_context);

							//Add it to the pass map
							m_passes_map.Insert(pass_name, std::unique_ptr<Pass>(pass));

							core::LogInfo("Created Pass <%s>", pass_name_string);
						}
						else
						{
							AddError(load_context, "Pass <%s> already exist, discarting new one", pass_name_string);
						}
					}
					else
					{
						AddError(load_context, "Pass inside the node <Passes> must have name attribute");
					}
				}
				else
				{
					AddError(load_context, "Only nodes <Pass> are supported inside the node <Passes>");
				}

				pass_element = pass_element->NextSiblingElement();
			}
		}
		return (load_context.errors.size() == 0);
	}

	bool System::AddResource(const ResourceName& name, std::unique_ptr<Resource>& resource, ResourceSource source)
	{
		auto& resource_it = m_resources_map[name];
		if (!resource_it)
		{
			m_resources_map.Insert(name, System::ResourceInfo(std::move(resource), source));
			return true;
		}
		else
		{
			resource.release();
			core::LogWarning("Game Resource <%s> has been already added, discarting the new resource", name.GetValue());
			return false;
		}
	}


	System * CreateRenderSystem(display::Device* device, job::System* job_system, platform::Game* game, const SystemDesc& desc)
	{
		System* system = new System();

		//Register all basic resources factories
		RegisterResourceFactory<BoolResource>(system);
		RegisterResourceFactory<TextureResource>(system);
		RegisterResourceFactory<ConstantBufferResource>(system);
		RegisterResourceFactory<VertexBufferResource>(system);
		RegisterResourceFactory<RenderTargetResource>(system);
		RegisterResourceFactory<RootSignatureResource>(system);
		RegisterResourceFactory<GraphicsPipelineStateResource>(system);
		RegisterResourceFactory<ComputePipelineStateResource>(system);
		RegisterResourceFactory<DescriptorTableResource>(system);

		//Register all basic passes factories
		RegisterPassFactory<ContextPass>(system);
		RegisterPassFactory<SetRenderTargetPass>(system);
		RegisterPassFactory<ClearRenderTargetPass>(system);
		RegisterPassFactory<SetRootSignaturePass>(system);
		RegisterPassFactory<SetRootConstantBufferPass>(system);
		RegisterPassFactory<SetRootShaderResourcePass>(system);
		RegisterPassFactory<SetRootUnorderedAccessBufferPass>(system);
		RegisterPassFactory<SetPipelineStatePass>(system);
		RegisterPassFactory<SetDescriptorTablePass>(system);
		RegisterPassFactory<DrawFullScreenQuadPass>(system);
		RegisterPassFactory<DrawRenderItemsPass>(system);

		system->m_device = device;
		system->m_job_system = job_system;
		system->m_game = game;

		//If there is a job system, means that there is a render thread, means game is needed
		assert(!system->m_job_system || (system->m_job_system && system->m_game));

		//Create render command list
		system->m_render_command_list = display::CreateCommandList(device, "RenderSystem");

		//Init gpu memory
		system->m_gpu_memory.Init(system->m_device, desc.static_gpu_memory_size, desc.dynamic_gpu_memory_size, desc.dynamic_gpu_memory_segment_size);

		//Register render gpu memory resources
		render::AddResource(system, "DynamicGPUMemory"_sh32, CreateResourceFromHandle<render::ShaderResourceResource>(display::WeakShaderResourceHandle(system->m_gpu_memory.m_dynamic_gpu_memory_buffer)));
		render::AddResource(system, "StaticGPUMemory"_sh32, CreateResourceFromHandle<render::UnorderedAccessBufferResource>(display::WeakUnorderedAccessBufferHandle(system->m_gpu_memory.m_static_gpu_memory_buffer)));

		return system;
	}

	void DestroyRenderSystem(System *& system, display::Device* device)
	{
		//Wait of the render task to be finished
		if (system->m_job_system)
		{
			job::Wait(system->m_job_system, g_render_fence);
		}

		//Destroy gpu memory
		system->m_gpu_memory.Destroy(system->m_device);

		//Destroy resources and passes
		DestroyResources(device, system->m_resources_map);
		DestroyPasses(device, system->m_passes_map);

		//Destroy command list
		display::DestroyHandle(device, system->m_render_command_list);


		delete system;

		system = nullptr;
	}

	bool LoadPassDescriptorFile(System* system, display::Device* device, const char* descriptor_file_buffer, size_t descriptor_file_buffer_size, std::vector<std::string>& errors)
	{
		//Destroy all cached contexts
		for (auto& render_context : system->m_cached_render_context)
		{
			auto& render_context_internal = render_context.render_context;
			system->DestroyRenderContext(render_context_internal);
		}
		system->m_cached_render_context.clear();

		//Only can be loaded if there are not context related to it
		if (system->m_render_context_pool.Size() > 0)
		{
			core::LogError("Errors loading render pass descriptor file, there are still old render context associated to the system");
			errors.emplace_back("Errors loading render pass descriptor file, there are still old render context associated to the system");
			return false;
		}

		//Save old resources in case the pass descriptor can not be load
		System::ResourceMap resources_map_old = std::move(system->m_resources_map);
		System::PassMap passes_map_old = std::move(system->m_passes_map);

		LoadContext load_context;
		load_context.device = device;
		load_context.render_system = system;

		bool success = system->Load(load_context, descriptor_file_buffer, descriptor_file_buffer_size);

		if (!success)
		{
			//Log the errors
			core::LogError("Errors loading render pass descriptor file");

			for (auto& error : load_context.errors)
			{
				core::LogError(error.c_str());
			}

			errors = std::move(load_context.errors);

			//Clear all resources created from the file
			DestroyResources(device, system->m_resources_map);
			DestroyPasses(device, system->m_passes_map);

			//Reset all values
			system->m_resources_map = std::move(resources_map_old);
			system->m_passes_map = std::move(passes_map_old);
		}
		else
		{
			//We still needs to get all resources that were defined by the game
			resources_map_old.VisitNamed([&](const ResourceName& name, auto& item)
			{
				if (item.source == ResourceSource::Game)
				{
					//Transfer resource to new resource map
					system->m_resources_map.Insert(name, item);
				}
			});

			//We can delete old resources and passes
			DestroyResources(device, resources_map_old);
			DestroyPasses(device, passes_map_old);

			core::LogInfo("Render pass descriptor file loaded");
		}

		return success;
	}


	RenderContextInternal * System::GetCachedRenderContext(const PassName & pass_name, uint16_t id, const PassInfo& pass_info)
	{
		for (auto& render_context : m_cached_render_context)
		{
			if (render_context.id == id && render_context.pass_name == pass_name)
			{
				return render_context.render_context;
			}
		}

		//Create one and add it in the activated list
		//Init resource map gets moved only here

		std::vector<std::string> errors;
		auto render_context = CreateRenderContext(m_device, pass_name, pass_info, errors);

		if (render_context)
		{
			m_cached_render_context.emplace_back(CachedRenderContext{id, pass_name, render_context});
		}

		return render_context;
	}

	void System::SubmitRender()
	{
		PROFILE_SCOPE("Render", "Submit", kRenderProfileColour);

		//Sync GPU memory resources
		m_gpu_memory.Sync(m_render_frame_index, display::GetLastCompletedGPUFrame(m_device));

		//Render thread
		display::BeginFrame(m_device);

		//Get render frame
		Frame& render_frame = m_frame_data;

		//Execute begin commands in the render_frame
		{
			PROFILE_SCOPE("Render", "ExecuteBeginCommands", kRenderProfileColour);

			auto render_context = display::OpenCommandList(m_device, m_render_command_list);

			render_frame.m_begin_frame_command_buffer.Visit([&](auto& data)
			{
				render::CommandBuffer::CommandOffset command_offset = 0;
				while (command_offset != render::CommandBuffer::InvalidCommandOffset)
				{
					command_offset = data.Execute(*render_context, command_offset);
				}
			});

			display::CloseCommandList(m_device, render_context);
			display::ExecuteCommandList(m_device, m_render_command_list);
		}

		//Sort all render items for each point of view, it can run in parallel
		for (auto& point_of_view : render_frame.m_point_of_views)
		{
			PROFILE_SCOPE("Render", "SortRenderItems", kRenderProfileColour);

			auto& render_items = point_of_view.m_render_items;
			auto& sorted_render_items = point_of_view.m_sorted_render_items;

			//Clear sort render items
			sorted_render_items.m_sorted_render_items.clear();

			//Copy render items from the point of view for each worker to the render context
			render_items.Visit([&](auto& data)
				{
					sorted_render_items.m_sorted_render_items.insert(sorted_render_items.m_sorted_render_items.end(), data.begin(), data.end());
				});

			//Sort render items
			std::sort(sorted_render_items.m_sorted_render_items.begin(), sorted_render_items.m_sorted_render_items.end(),
				[](const Item& a, const Item& b)
				{
					return a.full_32bit_sort_key < b.full_32bit_sort_key;
				});

			//Calculate begin/end for each render priority	
			sorted_render_items.m_priority_table.resize(m_render_priorities.size());
			size_t render_item_index = 0;
			const size_t num_sorted_render_items = sorted_render_items.m_sorted_render_items.size();

			for (size_t priority = 0; priority < m_render_priorities.size(); ++priority)
			{
				if (num_sorted_render_items > 0 && sorted_render_items.m_sorted_render_items[render_item_index].priority == priority)
				{
					//First item found
					sorted_render_items.m_priority_table[priority].first = render_item_index;

					//Look for the last one or the last 
					while (render_item_index < num_sorted_render_items && sorted_render_items.m_sorted_render_items[render_item_index].priority == priority)
					{
						render_item_index++;
					}

					//Last item found
					sorted_render_items.m_priority_table[priority].second = std::min(render_item_index, num_sorted_render_items - 1);
				}
				else
				{
					//We don't have any item of priority in the sort items
					sorted_render_items.m_priority_table[priority].first = sorted_render_items.m_priority_table[priority].second = -1;
				}
			}
		}

		//Sort all the render passes, it can be run in parallel and data is not needed until execution TODO

		//For each render pass, we could run it in parallel
		for (auto& render_pass : render_frame.m_render_passes)
		{
			PROFILE_SCOPE("Render", "SubmitRenderPasses", kRenderProfileColour);

			//Find the render_context associated to it
			RenderContextInternal* render_context = GetCachedRenderContext(render_pass.pass_name, 0, render_pass.pass_info);
			
			if (render_context)
			{
				
				if (render_pass.associated_point_of_view_name != PointOfViewName("None"))
				{
					//Find associated point of view
					for (auto& point_of_view : render_frame.m_point_of_views)
					{
						if (point_of_view.m_name == render_pass.associated_point_of_view_name &&
							point_of_view.m_id == render_pass.associated_point_of_view_id)
						{
							//Set point to view to the context
							render_context->m_point_of_view = &point_of_view;
							break;
						}
					}

					if (render_context->m_point_of_view == nullptr)
					{
						//Point of view was not found, as the render pass is removed I expect the graph to fail to be build
						core::LogWarning("Render pass <%s><%i> can not find associated point of view <%s><%i>, render pass removed",
							render_pass.pass_name, render_pass.id,
							render_pass.associated_point_of_view_name, render_pass.associated_point_of_view_id);

						continue;
					}
					
				}
				else
				{
					render_context->m_point_of_view = nullptr;
				}

				//Set pass info
				render_context->m_pass_info = render_pass.pass_info;

				{
					PROFILE_SCOPE("Render", "CapturePass", kRenderProfileColour);
					//Capture pass
					render_context->m_root_pass->Render(*render_context);
				}
				{
					PROFILE_SCOPE("Render", "RenderPass", kRenderProfileColour);
					//Execute pass
					render_context->m_root_pass->Execute(*render_context);
				}
			}
		}

		display::EndFrame(m_device);

		render_frame.Reset();

		if (m_game)
		{
			//We need to present from the render thread
			m_game->Present();
		}

		//Increase render index
		m_render_frame_index++;
	}

	void BeginPrepareRender(System * system)
	{
		if (system->m_job_system)
		{
			//Sync with the submit job
			job::Wait(system->m_job_system, g_render_fence);
		}
	}

	//Submit render job
	void SubmitRenderJob(void* data)
	{
		System* render_system = reinterpret_cast<System*>(data);

		render_system->SubmitRender();
	}

	void EndPrepareRenderAndSubmit(System * system)
	{
		//Render frame has all the information

		//Submit render if the job system is activated
		if (system->m_job_system)
		{
			assert(system->m_game);

			job::AddJob(system->m_job_system, SubmitRenderJob, system, g_render_fence);
		}
		else
		{
			//Submit
			system->SubmitRender();
		}

		//Increase game frame index
		system->m_game_frame_index++;
	}

	uint64_t GetGameFrameIndex(System* system)
	{
		return system->m_game_frame_index;
	}

	uint64_t GetRenderFrameIndex(System* system)
	{
		return system->m_render_frame_index;
	}

	Frame & GetGameRenderFrame(System * system)
	{
		return system->m_frame_data;
	}

	Priority GetRenderItemPriority(System * system, PriorityName priority_name)
	{
		const size_t priorities_size = system->m_render_priorities.size();
		for (size_t i = 0; i < priorities_size; ++i)
		{
			if (system->m_render_priorities[i] == priority_name)
				return static_cast<Priority>(i);
		}
		assert(priorities_size < 255);

		system->m_render_priorities.push_back(priority_name);
		return static_cast<Priority>(priorities_size);
	}

	AllocHandle AllocStaticGPUMemory(System* system, const size_t size, const void* data, const uint64_t frame_index)
	{
		AllocHandle handle = system->m_gpu_memory.m_static_gpu_memory_allocator.Alloc(size);

		if (data)
		{
			UpdateStaticGPUMemory(system, handle, data, size, frame_index);
		}

		return std::move(handle);
	}

	void DeallocStaticGPUMemory(System* system, AllocHandle&& handle, const uint64_t frame_index)
	{
		system->m_gpu_memory.m_static_gpu_memory_allocator.Dealloc(std::move(handle), frame_index);
	}

	void UpdateStaticGPUMemory(System* system, const AllocHandle& handle, const void* data, const size_t size, const uint64_t frame_index)
	{
		//Destination size needs to be aligned to float4
		constexpr size_t size_float4 = sizeof(float) * 4;
		assert(size > 0);

		size_t dest_size = (((size - 1) % size_float4) + 1) * size_float4;

		//Data gets copied in the dynamic gpu memory
		void* gpu_memory = AllocDynamicGPUMemory(system, dest_size, frame_index);
		memcpy(gpu_memory, data, size);

		//Calculate offsets
		uint8_t* dynamic_memory_base = reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(system->m_device, system->m_gpu_memory.m_dynamic_gpu_memory_buffer));

		uint32_t source_offset = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(gpu_memory) - dynamic_memory_base);
		const FreeListAllocation& destination_allocation = system->m_gpu_memory.m_static_gpu_memory_allocator.Get(handle);

		//Add copy command
		system->m_gpu_memory.AddCopyDataCommand(frame_index, source_offset, static_cast<uint32_t>(destination_allocation.offset), static_cast<uint32_t>(size));

	}

	void* AllocDynamicGPUMemory(System* system, const size_t size, const uint64_t frame_index)
	{
		size_t offset = system->m_gpu_memory.m_dynamic_gpu_memory_allocator.Alloc(size, frame_index);

		//Return the memory address inside the resource
		return reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(system->m_device, system->m_gpu_memory.m_dynamic_gpu_memory_buffer)) + offset;
	}

	display::WeakUnorderedAccessBufferHandle GetStaticGPUMemoryResource(System* system)
	{
		return system->m_gpu_memory.m_static_gpu_memory_buffer;
	}

	display::WeakShaderResourceHandle GetDynamicGPUMemoryResource(System* system)
	{
		return system->m_gpu_memory.m_dynamic_gpu_memory_buffer;
	}

	bool AddResource(System * system, const ResourceName& name, std::unique_ptr<Resource>&& resource)
	{
		return system->AddResource(name, resource, ResourceSource::Game);
	}

	bool RegisterResourceFactory(System * system, const RenderClassType& resource_type, std::unique_ptr<FactoryInterface<Resource>>& resource_factory)
	{
		auto& it = system->m_resource_factories_map[resource_type];
		if (it)
		{
			core::LogWarning("Resource <%s> has been already added, discarting new resource type", resource_type.GetValue());
			return false;
		}
		system->m_resource_factories_map.Insert(resource_type, std::move(resource_factory));
		return true;
	}

	bool RegisterPassFactory(System * system, const RenderClassType& pass_type, std::unique_ptr<FactoryInterface<Pass>>& pass_factory)
	{
		auto& it = system->m_resource_factories_map[pass_type];
		if (it)
		{
			core::LogWarning("Pass <%s> has been already added, discarting new pass type", pass_type.GetValue());
			return false;
		}

		system->m_pass_factories_map.Insert(pass_type, std::move(pass_factory));
		return true;
	}

	Resource * GetResource(System* system, const ResourceName& name)
	{
		auto& it = system->m_resources_map[name];
		if (it)
		{
			return it->resource.get();
		}
		return nullptr;
	}

	Pass * GetPass(System* system, const PassName& name)
	{
		auto& it = system->m_passes_map[name];
		if (it)
		{
			return it->get();
		}
		return nullptr;
	}

	ResourceName LoadContext::GetResourceReference(LoadContext & load_context)
	{
		//Check if is a resource
		if (auto xml_resource_element = load_context.current_xml_element->FirstChildElement("Resource"))
		{
			//It is a resource, load it using as prefix the pass name and return the name 
			load_context.current_xml_element = xml_resource_element;
			return load_context.render_system->LoadResource(load_context, load_context.pass_name);
		}
		else
		{
			//The resource is in the value
			return ResourceName(load_context.current_xml_element->GetText());
		}
	}

	bool LoadContext::AddResource(const ResourceName& name, std::unique_ptr<Resource>& resource)
	{
		return render_system->AddResource(name, resource, ResourceSource::PassDescriptor);
	}
}