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
#include <numeric>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

namespace
{
	template<class CONTAINER>
	void DestroyResources(display::Device* device, CONTAINER& container)
	{
		for (auto& [key, item] : container)
		{
			if (item && item->resource)
			{
				item->resource->Destroy(device);
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

	//Sync fence, it avoids the render frame been used before the render has been submited
	job::Fence g_render_fence;

	//Generate a pass resource name
	render::ResourceName CalculatePassResourceName(const render::ResourceName& name, const render::PassName& pass_name, uint16_t pass_id)
	{
		return core::HashConst<uint32_t>(name.GetHash() ^ pass_name.GetHash() ^ pass_id, "");
	}
}

namespace render
{
	Resource * RenderContext::GetResource(const ResourceName& name, bool& can_not_be_cached) const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);

		//First check it is a pass resource
		ResourceName pass_resource_name = CalculatePassResourceName(name, render_context->m_pass_name, render_context->m_pass_id);
		Resource* resource = render::GetResource(render_context->m_render_pass_system, pass_resource_name);
		
		if (resource)
		{
			can_not_be_cached = true;
			return resource;
		}
		else
		{
			can_not_be_cached = false;
			//Then check system resources
			ResourceSource source;
			Resource* resource = render_context->m_render_pass_system->GetResource(name, source);
			can_not_be_cached = (source == ResourceSource::Pass || source == ResourceSource::Pool);
			return resource;
		}
	}

	bool RenderContext::AddPassResource(const ResourceName& name, std::unique_ptr<Resource>&& resource)
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);

		//Mix the pass name and the id in the name of the resource
		ResourceName pass_resource_name = CalculatePassResourceName(name, render_context->m_pass_name, render_context->m_pass_id);

		return render_context->m_render_pass_system->AddResource(pass_resource_name, std::move(resource), ResourceSource::Pass);
	}

	Frame & RenderContext::GetRenderFrame()
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_render_pass_system->m_frame_data[render_context->m_render_pass_system->m_render_frame_index % 2];
	}

	const PointOfView* RenderContext::GetPointOfView() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_point_of_view;
	}

	ContextPass* RenderContext::GetContextRootPass() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_context_root_pass;
	}

	display::Device * RenderContext::GetDevice() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_display_device;
	}

	render::System* RenderContext::GetRenderSystem() const
	{
		auto render_context = reinterpret_cast<const RenderContextInternal*>(this);
		return render_context->m_render_pass_system;
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

	std::pair<std::unique_ptr<Resource>, display::TranstitionState> System::AllocPoolResource(ResourceName resource_name, PoolResourceType type, bool not_alias, uint16_t width, uint16_t height, uint32_t size, const display::Format& format, const float default_depth, const uint8_t default_stencil, const bool clear)
	{
		//Look in the pool for one free with the same parameters
		for (auto& pool_resource : m_pool_resources)
		{
			if (pool_resource.can_be_reuse
				&& pool_resource.name != ResourceName()
				&& (!not_alias || (pool_resource.name == resource_name && pool_resource.not_alias))
				&& pool_resource.type == type
				&& pool_resource.format == format
				&& pool_resource.width == width
				&& pool_resource.height == height
				&& pool_resource.size == size
				&& pool_resource.default_depth == default_depth
				&& pool_resource.default_stencil == default_stencil
				&& pool_resource.clear == clear)
			{
				assert(pool_resource.resource.get());
				//It can be use
				pool_resource.can_be_reuse = false;
				pool_resource.last_render_frame_used = m_render_frame_index;
				//TODO, change debug name

				//Give ownership to the resource info
				return std::make_pair<>(std::move(pool_resource.resource), pool_resource.m_access);
			}
		}

		//It needs to create a new resource to match the parameters

		std::unique_ptr<Resource> resource;
		display::TranstitionState access;
		switch (type)
		{
		case PoolResourceType::RenderTarget:
		{
			display::Texture2DDesc desc = display::Texture2DDesc::CreateRenderTarget(format, width, height);
			display::Texture2DHandle handle = display::CreateTexture2D(m_device, desc, resource_name.GetValue());

			resource = CreateResourceFromHandle<RenderTargetResource>(handle, width, height);
			access = resource->GetDefaultAccess();
		}
		break;
		case PoolResourceType::DepthBuffer:
		{
			display::Texture2DDesc desc = display::Texture2DDesc::CreateDepthBuffer(display::Format::D32_FLOAT, width, height, default_depth, default_stencil);
			display::Texture2DHandle handle = display::CreateTexture2D(m_device, desc, resource_name.GetValue());

			resource = CreateResourceFromHandle<DepthBufferResource>(handle);
			access = resource->GetDefaultAccess();
		}
		break;
		case PoolResourceType::Texture2D:
		{
			display::Texture2DDesc desc = display::Texture2DDesc::CreateTexture2D(display::Access::Static, format, width, height, 0, 0, 1, nullptr, true);
			display::Texture2DHandle handle = display::CreateTexture2D(m_device, desc, resource_name.GetValue());

			resource = CreateResourceFromHandle<TextureResource>(handle);
			access = resource->GetDefaultAccess();
		}
		break;
		case PoolResourceType::Buffer:
		{
			void* init_data = nullptr;
			if (clear)
			{
				//Needs to be clear
				init_data = new uint8_t[size];
				memset(init_data, 0, size);
			}
			assert(size % 4 == 0);
			display::BufferDesc desc = display::BufferDesc::CreateStructuredBuffer(display::Access::Static, size / 4, 4, true, init_data);
			display::BufferHandle handle = display::CreateBuffer(m_device, desc, resource_name.GetValue());

			resource = CreateResourceFromHandle<BufferResource>(handle);
			access = resource->GetDefaultAccess();

			if (init_data)
			{
				delete[] init_data;
			}
		}
		break;
		}

		//Look for empty slot (no name)
		for (auto& pool_resource : m_pool_resources)
		{
			if (pool_resource.name == ResourceName())
			{
				//Use this slot to add the new resource
				pool_resource = PoolResource{ {}, resource_name, type, width, height, size, format, default_depth, default_stencil, clear, false, not_alias, m_render_frame_index, access};

				return std::make_pair<>(std::move(resource), access);
			}
		}

		//Add into the pool
		m_pool_resources.emplace_back(PoolResource{ {}, resource_name, type, width, height, size, format, default_depth, default_stencil, clear, false, not_alias, m_render_frame_index, access });

		return std::make_pair<>(std::move(resource), access);
	}

	void System::DeallocPoolResource(ResourceName resource_name, std::unique_ptr<Resource>& resource, const display::TranstitionState access)
	{
		//Just return it to the pool
		for (auto& pool_resource : m_pool_resources)
		{
			if (pool_resource.name == resource_name &&
				pool_resource.can_be_reuse == false)
			{
				assert(pool_resource.can_be_reuse == false);
				assert(pool_resource.resource.get() == nullptr);
				
				//Return to the pool
				pool_resource.resource = std::move(resource);
				pool_resource.m_access = access; //Update state
				pool_resource.can_be_reuse = true;
				return;
			}
		}

		core::LogError("Pool resource <%s> has been ask for release but it has not been allocated as pool resource", resource_name.GetValue());
	}

	void System::UpdatePoolResources()
	{
		//Loop back and check for resources that has not be used for the last two frames
		size_t i = m_pool_resources.size();
		while (i != 0)
		{
			i--;
			auto& pool_resource = m_pool_resources[i];
			if (pool_resource.name != ResourceName() &&
				(pool_resource.last_render_frame_used + 2) < m_render_frame_index)
			{
				//Resource can be release
				assert(pool_resource.can_be_reuse);

				pool_resource.resource->Destroy(m_device);
				pool_resource.resource = nullptr;

				//We free the slot just making the name empty
				pool_resource.name = ResourceName();
			}
		}
	}

	RenderContextInternal * System::CreateRenderContext(display::Device * device, const PassName & pass_name, const uint16_t pass_id, const PassInfo & pass_info, std::vector<std::string>& errors)
	{
		//Get pass
		auto render_pass = GetPass(this, pass_name);
		if (render_pass && render_pass->Type() == RenderClassType("Pass"_sh32))
		{
			//Create Render Context
			RenderContextInternal* render_context = m_render_context_pool.Alloc(this, device, pass_name, pass_id, pass_info, dynamic_cast<ContextPass*>(render_pass));

			ErrorContext errors_context;

			//Allow the passes to init the render context 
			render_pass->InitPass(*render_context, device, errors_context);

			errors = std::move(errors_context.errors);

			if (errors.empty())
			{
				core::LogInfo("Created a render pass <%s> from definition pass", pass_name.GetValue());
				return render_context;
			}
			else
			{
				core::LogError("Errors creating a render pass <%s> from definition pass", pass_name.GetValue());
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
			core::LogError("Errors creating a render pass <%s>, definition pass doesn't exist or it is not a context pass", pass_name.GetValue());
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
					const char* pass_group_name_string = pass_element->Attribute("group");
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

							//Check if it is part of a group
							if (pass_group_name_string)
							{
								GroupPassName group_name(pass_group_name_string);
								auto it = m_group_passes_map.Find(group_name);
								if (it)
								{
									(*it).push_back(pass_name);
								}
								else
								{
									m_group_passes_map.Insert(group_name, std::vector<PassName>{ pass_name });
								}
							}

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

	bool System::AddResource(const ResourceName& name, std::unique_ptr<Resource>&& resource, ResourceSource source, const std::optional<display::TranstitionState>& current_access)
	{
		display::TranstitionState init_state;
		if (current_access.has_value())
		{
			init_state = current_access.value();
		}
		else
		{
			if (resource)
			{
				//Use default for the resource type
				init_state = resource->GetDefaultAccess();
			}
			else
			{
				init_state = display::TranstitionState::Common;
			}
		}

		auto& resource_it = m_resources_map[name];
		if (!resource_it)
		{
			m_resources_map.Insert(name, std::make_unique<System::ResourceInfo>(resource, source, init_state));
			return true;
		}
		else
		{
			if (source != ResourceSource::Pool)
			{
				resource.release();
				core::LogWarning("Game Resource <%s> has been already added, discarting the new resource", name.GetValue());
				return false;
			}
			else
			{
				//Pool resources can be added more than once, as each pass add one
				return true;
			}
		}
	}

	Resource* System::GetResource(const ResourceName& name, ResourceSource& source)
	{
		auto& it = m_resources_map[name];
		if (it)
		{
			source = (*it)->source;
			return (*it)->resource.get();
		}
		return nullptr;
	}


	System * CreateRenderSystem(display::Device* device, job::System* job_system, platform::Game* game, const SystemDesc& desc)
	{
		System* system = new System();

		//Register all basic resources factories
		RegisterResourceFactory<BoolResource>(system);
		RegisterResourceFactory<TextureResource>(system);
		RegisterResourceFactory<ConstantBufferResource>(system);
		RegisterResourceFactory<RenderTargetResource>(system);
		RegisterResourceFactory<RootSignatureResource>(system);
		RegisterResourceFactory<GraphicsPipelineStateResource>(system);
		RegisterResourceFactory<ComputePipelineStateResource>(system);
		RegisterResourceFactory<DescriptorTableResource>(system);

		//Register all basic passes factories
		RegisterPassFactory<ContextPass>(system);
		RegisterPassFactory<SetRenderTargetPass>(system);
		RegisterPassFactory<ClearRenderTargetPass>(system);
		RegisterPassFactory<ClearDepthStencilPass>(system);
		RegisterPassFactory<SetRootSignaturePass>(system);
		RegisterPassFactory<SetRootConstantBufferPass>(system);
		RegisterPassFactory<SetRootShaderResourcePass>(system);
		RegisterPassFactory<SetRootUnorderedAccessBufferPass>(system);
		RegisterPassFactory<SetPipelineStatePass>(system);
		RegisterPassFactory<SetComputePipelineStatePass>(system);
		RegisterPassFactory<SetDescriptorTablePass>(system);
		RegisterPassFactory<DrawFullScreenQuadPass>(system);
		RegisterPassFactory<DispatchViewComputePass>(system);
		RegisterPassFactory<DispatchComputePass>(system);
		RegisterPassFactory<DrawRenderItemsPass>(system);

		system->m_device = device;
		system->m_job_system = job_system;
		system->m_game = game;

		//If there is a job system, means that there is a render thread, means game is needed
		assert(!system->m_job_system || (system->m_job_system && system->m_game));

		//Create render command list
		system->m_render_command_list = display::CreateCommandList(device, "RenderSystem");

		//Register the back buffer
		render::AddGameResource(system, "BackBuffer"_sh32, CreateResourceFromHandle<render::RenderTargetResource>(display::GetBackBuffer(device)), display::TranstitionState::Present);

		//Create a job allocator if there is a job system
		if (system->m_job_system)
		{
			system->m_job_allocator = std::make_unique<job::JobAllocator<1024 * 1024>>();
		}
		return system;
	}

	void DestroyRenderSystem(System *& system, display::Device* device)
	{
		//Wait of the render task to be finished
		if (system->m_job_system)
		{
			job::Wait(system->m_job_system, g_render_fence);
		}

		//Destroy resources and passes
		DestroyResources(device, system->m_resources_map);
		DestroyPasses(device, system->m_passes_map);
		
		//Destroy pool resources
		for (auto& pool_resource : system->m_pool_resources)
		{
			if (pool_resource.resource.get())
			{
				pool_resource.resource->Destroy(device);
				pool_resource.resource = nullptr;
			}
		}
		

		//Destroy command list
		display::DestroyHandle(device, system->m_render_command_list);

		//Destroy modules
		for (auto& [key, module] : system->m_modules)
		{
			module->Shutdown(device, system);
		}

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

		//Destroy pool resources, they will get recreated
		//Destroy pool resources
		for (auto& pool_resource : system->m_pool_resources)
		{
			if (pool_resource.resource.get())
			{
				pool_resource.resource->Destroy(device);
				pool_resource.resource = nullptr;
			}
		}
		system->m_pool_resources.clear();

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
				if (item->source == ResourceSource::Game)
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
		auto render_context = CreateRenderContext(m_device, pass_name, id, pass_info, errors);

		if (render_context)
		{
			m_cached_render_context.push_back({id, pass_name, render_context});
		}

		return render_context;
	}

	void System::SubmitRender()
	{
		PROFILE_SCOPE("Render", kRenderProfileColour, "Submit");
		
		if (m_job_system)
		{
			//Reset job allocators
			m_job_allocator->Clear();
		}

		//Render thread
		display::BeginFrame(m_device);

		for (auto& [key, module] : m_modules)
		{
			module->BeginFrame(m_device, this, m_render_frame_index, display::GetLastCompletedGPUFrame(m_device));
		}

		//Get render frame
		Frame& render_frame = m_frame_data[m_render_frame_index % 2];

		//Vector of all command list to be executed at the end of the render
		std::vector<display::WeakCommandListHandle> command_list_to_execute;

		//Execute begin commands in the render_frame
		{
			PROFILE_SCOPE("Render", kRenderProfileColour, "ExecuteBeginCommands");

			auto render_context = display::OpenCommandList(m_device, m_render_command_list);

			render_frame.m_begin_frame_command_buffer.Visit([&](auto& data)
			{
				render::CommandBuffer::CommandOffset command_offset = 0;
				while (command_offset.IsValid())
				{
					command_offset = data.Execute(*render_context, command_offset);
				}
			});

			display::CloseCommandList(m_device, render_context);
			
			command_list_to_execute.push_back(m_render_command_list);
		}

		//Sort all render items for each point of view, it can run in parallel
		for (auto& point_of_view : render_frame.m_point_of_views)
		{
			PROFILE_SCOPE("Render", kRenderProfileColour, "SortRenderItems");

			auto& render_items = point_of_view.m_render_items;
			auto& sorted_render_items = point_of_view.m_sorted_render_items;

			//Clear sort render items
			sorted_render_items.m_sorted_render_items.clear();

			//Get number of render items
			size_t num_render_items = 0;
			render_items.Visit([&](auto& data)
				{
					num_render_items += data.size();
				});

			if (!m_job_system || !m_parallel_sort_render_items || (num_render_items < m_parallel_sort_render_item_min_count))
			{
				//Sort in the render job, not a lot 

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
			}
			else
			{
				job::Fence sorting_fence;
				//Sort each thread data array in a task and merge sort the result
				render_items.Visit([&](auto& data)
					{
						job::AddLambdaJob(m_job_system, [&data]()
							{
								PROFILE_SCOPE("Render", kRenderProfileColour, "SortRenderItemsJob");
								std::sort(data.begin(), data.end(),
									[](const Item& a, const Item& b)
									{
										return a.full_32bit_sort_key < b.full_32bit_sort_key;
									});
							}, m_job_allocator, sorting_fence);
					});

				//Merge sort the result
				sorted_render_items.m_sorted_render_items.resize(num_render_items);

				job::Wait(m_job_system, sorting_fence);

				struct SourceData
				{
					std::vector<render::Item>& data;
					size_t next_index;
					size_t size;
					SourceData(std::vector<render::Item>& _data) : data(_data)
					{
						next_index = 0;
						size = data.size();
					}
				};
					
				//Indicate the next position to merge for each sorted source data
				std::vector<SourceData> sorted_source_data;
				sorted_source_data.reserve(8);
				render_items.Visit([&](auto& data)
					{
						sorted_source_data.emplace_back(data);
					});

				const size_t num_sorted_source_data = sorted_source_data.size();
				bool all_empty = false;
				size_t sorted_render_items_index = 0;
				{
					PROFILE_SCOPE("Render", kRenderProfileColour, "MergedSortRenderItems");
					while (!all_empty)
					{
						render::Item next_render_item(0xFF, 0xFFFFFF, 0); //Worst case
						size_t next_item_sorted_data_index = static_cast<size_t>(-1);
						for (size_t i = 0; i < num_sorted_source_data; ++i)
						{
							auto& sorted_source = sorted_source_data[i];
							if (sorted_source.next_index < sorted_source.size)
							{
								if (sorted_source.data[sorted_source.next_index].full_32bit_sort_key < next_render_item.full_32bit_sort_key)
								{
									next_item_sorted_data_index = i;
									next_render_item = sorted_source.data[sorted_source.next_index];
								}
							}
						}

						if (next_item_sorted_data_index != static_cast<size_t>(-1))
						{
							//Found, add into the dest buffer
							sorted_render_items.m_sorted_render_items[sorted_render_items_index++] = next_render_item;
							//Increase the index for that sorted data
							sorted_source_data[next_item_sorted_data_index].next_index++;
						}
						else
						{
							//Done, if we didn't found any, means that all is done
							all_empty = true;
						}
					}
				}
			}

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
					sorted_render_items.m_priority_table[priority].second = (render_item_index - 1);
				}
				else
				{
					//We don't have any item of priority in the sort items
					sorted_render_items.m_priority_table[priority].first = sorted_render_items.m_priority_table[priority].second = -1;
				}
			}
		}

		//Expand group render passes and auto
		auto it = m_group_passes_map.Find("Auto"_sh32);
		if (it)
		{
			for (auto& pass_name : *it)
			{
				render_frame.m_render_passes.push_back(render::RenderPass{ pass_name, 0, PassInfo(), PointOfViewName(""_sh32), 0});
			}
		}
		for (auto& group_pass : render_frame.m_group_render_passes)
		{
			auto it = m_group_passes_map.Find(group_pass.group_pass_name);
			for (auto& pass_name : *it)
			{
				render_frame.m_render_passes.push_back(render::RenderPass{ pass_name, group_pass.id, group_pass.pass_info, group_pass.associated_point_of_view_name, group_pass.associated_point_of_view_id });
			}
		}

		//Cached render contexts
		std::vector<RenderContextInternal*> render_pass_contexts(render_frame.m_render_passes.size());
		for (size_t i = 0; i < render_frame.m_render_passes.size(); ++i)
		{
			auto& render_pass = render_frame.m_render_passes[i];
			render_pass_contexts[i] = GetCachedRenderContext(render_pass.pass_name, 0, render_pass.pass_info);
		}

		//Sort all the render passes, it can be run in parallel and data is not needed until execution
		std::vector<size_t> render_passes_sorted;
		bool render_graph_built = true;
		{
			render_passes_sorted.reserve(render_frame.m_render_passes.size());
			//All resources are state "Init"
			for (auto& [key, resource_info] : m_resources_map)
			{
				resource_info->state = "Init"_sh32;
			}

			std::vector<size_t> render_passes_to_process(render_frame.m_render_passes.size());
			std::iota(render_passes_to_process.begin(), render_passes_to_process.end(), 0);

			//Add passes into the sorted array solving all dependencies
			while (render_graph_built && render_passes_to_process.size() > 0)
			{
				size_t num_render_passes_left = render_passes_to_process.size();

				//Deferred update states
				//All actived passes will add here all the state updates, so all are accumulated until no more passes can be activated
				std::vector<ResourceStateSync> deferred_update_states;

				size_t pass_processed = -1;
				//Check if any pass is able to run
				for (auto& pass_index : render_passes_to_process)
				{
					auto& render_pass = render_frame.m_render_passes[pass_index];

					bool all_dependencies_passed = true;

					//Check the dependencies
					auto& dependencies = render_pass_contexts[pass_index]->GetContextRootPass()->GetPreResourceCondition();
					for (auto& dependency : dependencies)
					{
						if (dependency.resource.Get(this)->state != dependency.state)
						{
							all_dependencies_passed = false;
							break;
						}
					}

					if (all_dependencies_passed)
					{
						pass_processed = pass_index;
						break;
						
					}
				}

				if (pass_processed != -1)
				{
					//This pass can be run as all the dependencies are in a correct state
					render_passes_sorted.push_back(pass_processed);
					render_passes_to_process.erase(std::find(render_passes_to_process.begin(), render_passes_to_process.end(), pass_processed));

					//Add update states to the deferred list
					auto& update_states = render_pass_contexts[pass_processed]->GetContextRootPass()->GetPostUpdateCondition();

					//Update all deferred states
					for (auto& update_state : update_states)
					{
						update_state.resource.Get(this)->state = update_state.state;
					}
				}
				else
				{
					//The depedency graph can not be built, no render
					core::LogError("The render graph can not be built because the depedencies can not be match. Render is cancel.");
					core::LogError("Passes added to render in order <%d>", render_passes_sorted.size());
					for (const auto& index : render_passes_sorted)
					{
						auto& render_pass = render_frame.m_render_passes[index];
						core::LogError("Pass <%s>, ID<%d>", render_pass.pass_name.GetValue(), render_pass.id);
					}
					core::LogError("Resources states");
					for (auto& [key, resource_info] : m_resources_map)
					{
						core::LogError("	Resource <%s>, State <%s>", key.GetValue(), resource_info->state.GetValue());
					}
					core::LogError("Passes that could not render:", render_passes_to_process.size());
					for (const auto& index : render_passes_to_process)
					{
						auto& render_pass = render_frame.m_render_passes[index];
						core::LogError("Pass <%s>, ID<%d>", render_pass.pass_name.GetValue(), render_pass.id);
						auto& dependencies = render_pass_contexts[index]->GetContextRootPass()->GetPreResourceCondition();
						for (const auto& dependency : dependencies)
							core::LogError("	Depends of <%s>, State <%s>, State Requested <%s>", dependency.resource.GetResourceName().GetValue(), dependency.resource.Get(this)->state.GetValue(), dependency.state.GetValue());
					}

					render_graph_built = false;
					break;
				}
			}
		}

		if (render_graph_built)
		{
			PROFILE_SCOPE("Render", kRenderProfileColour, "SubmitRenderPasses");

			//For each render pass, we could run it in parallel
			for (auto& sorted_render_pass_index : render_passes_sorted)
			{
				auto& render_pass = render_frame.m_render_passes[sorted_render_pass_index];

				//Find the render_context associated to it
				RenderContextInternal* render_context = render_pass_contexts[sorted_render_pass_index];

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
				}
				else
				{
					render_context->m_point_of_view = nullptr;
				}

				//Set pass info
				render_context->m_pass_info = render_pass.pass_info;

				{
					PROFILE_SCOPE("Render", kRenderProfileColour, "CapturePass");

					//Request new pool resources
					for (auto& pool_resource : render_context->GetContextRootPass()->GetResourcePoolDependencies())
					{
						if (pool_resource.needs_to_allocate)
						{
							uint16_t width;
							uint16_t height;
							uint32_t size;
							if (pool_resource.type == PoolResourceType::DepthBuffer || pool_resource.type == PoolResourceType::RenderTarget || pool_resource.type == PoolResourceType::Texture2D)
							{
								if (pool_resource.width == 0 || pool_resource.height == 0)
								{
									//Calculate the resolution needed from the pass info
									width = render_context->m_pass_info.width * pool_resource.width_factor / 256;
									height = render_context->m_pass_info.height * pool_resource.height_factor / 256;

									//Adjust to the tile size
									width = (((width - 1) / pool_resource.tile_size_width) + 1) * pool_resource.tile_size_width;
									height = (((height - 1) / pool_resource.tile_size_height) + 1) * pool_resource.tile_size_height;
								}
								else
								{
									width = static_cast<uint16_t>(pool_resource.width);
									height = static_cast<uint16_t>(pool_resource.height);
								}
							}
							else if (pool_resource.type == PoolResourceType::Buffer)
							{
								size = pool_resource.size;
							}

							//Pass the control to the resource in the resource map
							auto allocated_pool_resource = AllocPoolResource(pool_resource.name, pool_resource.type, pool_resource.not_alias, width, height, size, pool_resource.format, pool_resource.default_depth, pool_resource.default_stencil, pool_resource.clear);
							auto resource_info = m_resources_map.Find(pool_resource.name)->get();
							resource_info->resource = std::move(allocated_pool_resource.first);
							resource_info->access = allocated_pool_resource.second;
						}
						else
						{
							ResourceSource source;
							if (GetResource(pool_resource.name, source) == nullptr)
							{
								core::LogError("Pool resource <%s> used during render pass <%s><%i> but the resource is not active",
									pool_resource.name.GetValue(),
									render_pass.pass_name.GetValue(), render_pass.id);
							}
						}
					}

					//Add resource barriers if needed
					std::vector<display::ResourceBarrier> resource_barriers_to_execute;
					resource_barriers_to_execute.reserve(render_context->GetContextRootPass()->GetResourceBarriers().size());
					for (auto& resource_barrier : render_context->GetContextRootPass()->GetResourceBarriers())
					{
						auto current_access = resource_barrier.resource.Get(this)->access;
						auto next_access = resource_barrier.access;

						if (current_access != next_access)
						{
							const auto& resource_info = resource_barrier.resource.Get(this);
							auto resource_handle = resource_info->resource->GetDisplayHandle();
							resource_info->access = next_access;

							std::visit(
								overloaded
								{
									[&](const display::WeakBufferHandle& handle)
									{
										resource_barriers_to_execute.emplace_back(handle, current_access, next_access);
									},
									[&](const display::WeakTexture2DHandle& handle)
									{
										resource_barriers_to_execute.emplace_back(handle, current_access, next_access);
									},
									[&](const std::monostate& handle)
									{
									}
								}, resource_handle);	
						}
						
					}

					//Capture pass
					render_context->GetContextRootPass()->RootContextRender(*render_context, resource_barriers_to_execute);

					//Free pool resources
					for (auto& pool_resource : render_context->GetContextRootPass()->GetResourcePoolDependencies())
					{
						if (pool_resource.will_be_free)
						{
							//Return to the pool
							DeallocPoolResource(pool_resource.name, m_resources_map.Find(pool_resource.name)->get()->resource, m_resources_map.Find(pool_resource.name)->get()->access);
						}
					}
					//Add to execute
					command_list_to_execute.push_back(render_context->GetContextRootPass()->GetCommandList());
				}
			}
		}

		if (command_list_to_execute.size() > 0)
		{
			display::ExecuteCommandLists(m_device, command_list_to_execute);
		}

		for (auto& [key, module] : m_modules)
		{
			module->EndFrame(m_device, this);
		}

		display::EndFrame(m_device);

		render_frame.Reset();

		UpdatePoolResources();

		if (m_game)
		{
			//We need to present from the render thread
			m_game->Present();
		}

		//We need to move the back buffer to present
		m_resources_map["BackBuffer"_sh32]->get()->access = display::TranstitionState::Present;

		//Increase render index
		m_render_frame_index++;
	}

	void BeginPrepareRender(System * system)
	{
	}

	void FlushAndWait(System* system)
	{
		if (system->m_job_system)
		{
			//Sync with the submit job
			job::Wait(system->m_job_system, g_render_fence);
		}
	}

	job::Fence* GetRenderFence(System* system)
	{
		return &g_render_fence;
	}

	//Submit render job
	void SubmitRenderJob(void* data)
	{
		System* render_system = reinterpret_cast<System*>(data);

		render_system->SubmitRender();
	}

	void EndPrepareRenderAndSubmit(System * system)
	{
		//Only one render job can be running, waiting here
		if (system->m_job_system)
		{
			//Sync with the submit job
			job::Wait(system->m_job_system, g_render_fence);
		}

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
		return system->m_frame_data[system->m_game_frame_index % 2];
	}

	Priority GetRenderItemPriority(System * system, const PriorityName priority_name)
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

	Module* GetModule(System* system, const ModuleName name)
	{
		return system->m_modules[name]->get();
	}

	void RegisterModule(System* system, const ModuleName name, std::unique_ptr<Module>&& module)
	{
		//Init module
		module->Init(system->m_device, system);

		system->m_modules.Insert(name, std::move(module));
	}

	void DisplayImguiStats(System* system, bool* activated)
	{
		if (ImGui::Begin("Render", activated, ImGuiWindowFlags_AlwaysAutoResize))
		{
			auto& points_of_view = system->m_frame_data[system->m_render_frame_index % 2].m_point_of_views;
			ImGui::Text("Num of point of views (%zu)", points_of_view.size());
			ImGui::Separator();
			ImGui::Checkbox("Parallel sort render items", &system->m_parallel_sort_render_items);
			ImGui::DragScalar("Parallel sort render items min count", ImGuiDataType_U32, &system->m_parallel_sort_render_item_min_count, 1.f);
			ImGui::Separator();
			for (auto& point_of_view : points_of_view)
			{
				ImGui::Text("Point of View (%s): Num of render items (%zu)", point_of_view.m_name.GetValue(), point_of_view.GetSortedRenderItems().m_sorted_render_items.size());
			}
			ImGui::Separator();
			for (const auto& module : system->m_modules)
			{
				module.second->DisplayImguiStats();
			}
			ImGui::End();
		}
	}

	bool AddGameResource(System * system, const ResourceName& name, std::unique_ptr<Resource>&& resource, const std::optional<display::TranstitionState>& current_access)
	{
		return system->AddResource(name, std::move(resource), ResourceSource::Game, current_access);
	}

	bool AddGameResource(System* system, const ResourceName& name, const PassName& pass_name, const uint16_t pass_id, std::unique_ptr<Resource>&& resource, const std::optional<display::TranstitionState>& current_access)
	{
		//Calculate new hash for this resource
		return system->AddResource(CalculatePassResourceName(name, pass_name, pass_id), std::move(resource), ResourceSource::Game, current_access);
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

	Resource* GetResource(System* system, const ResourceName& name)
	{
		ResourceSource source;
		return system->GetResource(name, source);
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
		return render_system->AddResource(name, std::move(resource), ResourceSource::PassDescriptor);
	}

	bool LoadContext::AddPoolResource(const ResourceName& name)
	{	
		//Gets added empty, the resource will get assigned during rendering
		return render_system->AddResource(name, {}, ResourceSource::Pool);
	}
}