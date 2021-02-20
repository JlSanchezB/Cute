#include "render_module_gpu_memory.h"
#include "display/display.h"
#include "render/render_resource.h"
#include "render/render_helper.h"

namespace render
{

	GPUMemoryRenderModule::GPUMemoryRenderModule(const GPUMemoryDesc& desc) :
		m_static_gpu_memory_size(desc.static_gpu_memory_size), m_dynamic_gpu_memory_size(desc.dynamic_gpu_memory_size), m_dynamic_gpu_memory_segment_size(desc.dynamic_gpu_memory_segment_size)
	{
	}

	void GPUMemoryRenderModule::Init(display::Device* device, System* system)
	{
		assert(m_static_gpu_memory_size % 16 == 0);
		assert(m_dynamic_gpu_memory_size % 16 == 0);

		//Init static buffer
		display::UnorderedAccessBufferDesc static_buffer_desc;
		static_buffer_desc.element_size = 16;
		static_buffer_desc.element_count = m_static_gpu_memory_size / 16;

		m_static_gpu_memory_buffer = display::CreateUnorderedAccessBuffer(device, static_buffer_desc, "StaticGpuMemoryBuffer");
		
		//Init static allocator
		m_static_gpu_memory_allocator.Init(m_static_gpu_memory_size);

		//Init dynamic buffer
		display::ShaderResourceDesc dynamic_buffer_desc;
		dynamic_buffer_desc.access = display::Access::Upload;
		dynamic_buffer_desc.type = display::ShaderResourceType::Buffer;
		dynamic_buffer_desc.size = m_dynamic_gpu_memory_size;
		dynamic_buffer_desc.num_elements = m_dynamic_gpu_memory_size / 16;
		dynamic_buffer_desc.structure_stride = 16;

		m_dynamic_gpu_memory_buffer = display::CreateShaderResource(device, dynamic_buffer_desc, "DynamicGpuMemoryBuffer");
		m_dynamic_gpu_memory_base_ptr = reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(device, m_dynamic_gpu_memory_buffer));

		//Init dynamic allocator
		m_dynamic_gpu_memory_allocator.Init(m_dynamic_gpu_memory_size, m_dynamic_gpu_memory_segment_size);


		//Copy data compute
		{
			//Create compute root signature
			display::RootSignatureDesc root_signature_desc;
			root_signature_desc.num_root_parameters = 3;
			root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::UnorderedAccessBuffer;
			root_signature_desc.root_parameters[0].root_param.shader_register = 0;
			root_signature_desc.root_parameters[0].visibility = display::ShaderVisibility::All;

			root_signature_desc.root_parameters[1].type = display::RootSignatureParameterType::ShaderResource;
			root_signature_desc.root_parameters[1].root_param.shader_register = 0;
			root_signature_desc.root_parameters[1].visibility = display::ShaderVisibility::All;

			root_signature_desc.root_parameters[2].type = display::RootSignatureParameterType::Constants;
			root_signature_desc.root_parameters[2].visibility = display::ShaderVisibility::All;
			root_signature_desc.root_parameters[2].root_param.num_constants = 2;
			root_signature_desc.root_parameters[2].root_param.shader_register = 0;

			root_signature_desc.num_static_samplers = 0;

			//Create the root signature
			m_copy_data_compute_root_signature = display::CreateRootSignature(device, root_signature_desc, "Copy Data Compute");
		}

		{
			//Compute pipeline, compile shader for testing
			const char* shader_code =
				"RWStructuredBuffer<uint4> destination_buffer : u0; \
					 StructuredBuffer<uint4> source_buffer : t0; \
					 uint4 parameters : b0; \
					 \
					[numthreads(64, 1, 1)]\
					void copy_data(uint3 thread : SV_DispatchThreadID)\
					{\
						if (thread.x < parameters.y)\
						{ \
							uint2 copy_data;\
							if ((thread.x % 2) == 0)\
								copy_data = source_buffer[parameters.x + thread.x/2].xy;\
							else\
								copy_data = source_buffer[parameters.x + thread.x/2].zw; \
							\
							destination_buffer[copy_data.y] = source_buffer[copy_data.x];\
						}\
					};";

			std::vector<char> compute_shader;

			display::CompileShaderDesc compile_shader_desc;
			compile_shader_desc.code = shader_code;
			compile_shader_desc.entry_point = "copy_data";
			compile_shader_desc.target = "cs_5_0";

			display::CompileShader(device, compile_shader_desc, compute_shader);

			//Create pipeline state
			display::ComputePipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_copy_data_compute_root_signature;

			//Add shaders
			pipeline_state_desc.compute_shader.data = reinterpret_cast<void*>(compute_shader.data());
			pipeline_state_desc.compute_shader.size = compute_shader.size();

			//Create
			m_copy_data_compute_pipeline_state = display::CreateComputePipelineState(device, pipeline_state_desc, "Copy Data Compute");

			//Register resources
			AddGameResource(system, "StaticGPUMemoryBuffer"_sh32, CreateResourceFromHandle<render::UnorderedAccessBufferResource>(display::WeakUnorderedAccessBufferHandle(m_static_gpu_memory_buffer)));
			AddGameResource(system, "DynamicGPUMemoryBuffer"_sh32, CreateResourceFromHandle<render::ShaderResourceResource>(display::WeakShaderResourceHandle(m_dynamic_gpu_memory_buffer)));


			//Register pass
			render::RegisterPassFactory<SyncStaticGPUMemoryPass>(system);
		}
	}

	void GPUMemoryRenderModule::Shutdown(display::Device* device, System* system)
	{
		display::DestroyHandle(device, m_static_gpu_memory_buffer);
		display::DestroyHandle(device, m_dynamic_gpu_memory_buffer);
		display::DestroyHandle(device, m_copy_data_compute_root_signature);
		display::DestroyHandle(device, m_copy_data_compute_pipeline_state);
	}

	void GPUMemoryRenderModule::BeginFrame(display::Device* device, System* system, uint64_t cpu_frame_index, uint64_t freed_frame_index)
	{
		m_dynamic_gpu_memory_allocator.Sync(cpu_frame_index, freed_frame_index);
		m_static_gpu_memory_allocator.Sync(freed_frame_index);
	}

	void* GPUMemoryRenderModule::AllocDynamicGPUMemory(display::Device* device, const size_t size, const uint64_t frame_index)
	{
		size_t offset = m_dynamic_gpu_memory_allocator.Alloc(size, frame_index);

		//Return the memory address inside the resource
		return reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(device, m_dynamic_gpu_memory_buffer)) + offset;
	}

	AllocHandle GPUMemoryRenderModule::AllocStaticGPUMemory(display::Device* device, const size_t size, const void* data, const uint64_t frame_index)
	{
		AllocHandle handle = m_static_gpu_memory_allocator.Alloc(size);

		if (data)
		{
			UpdateStaticGPUMemory(device, handle, data, size, frame_index);
		}

		return std::move(handle);
	}

	void GPUMemoryRenderModule::DeallocStaticGPUMemory(display::Device* device, AllocHandle&& handle, const uint64_t frame_index)
	{
		m_static_gpu_memory_allocator.Dealloc(std::move(handle), frame_index);
	}

	void GPUMemoryRenderModule::UpdateStaticGPUMemory(display::Device* device, const AllocHandle& handle, const void* data, const size_t size, const uint64_t frame_index)
	{
		assert(size > 0);

		//Destination size needs to be aligned to float4
		size_t dest_size = (((size - 1) >> 4) + 1) << 4;

		//Data gets copied in the dynamic gpu memory
		void* gpu_memory = AllocDynamicGPUMemory(device, dest_size, frame_index);
		memcpy(gpu_memory, data, size);

		//Calculate offsets
		uint8_t* dynamic_memory_base = reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(device, m_dynamic_gpu_memory_buffer));

		uint32_t source_offset = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(gpu_memory) - dynamic_memory_base);
		const FreeListAllocation& destination_allocation = m_static_gpu_memory_allocator.Get(handle);

		//Add copy command
		AddCopyDataCommand(frame_index, source_offset, static_cast<uint32_t>(destination_allocation.offset), static_cast<uint32_t>(size));

	}

	display::WeakUnorderedAccessBufferHandle GPUMemoryRenderModule::GetStaticGPUMemoryResource()
	{
		return m_static_gpu_memory_buffer;
	}

	size_t GPUMemoryRenderModule::GetStaticGPUMemoryOffset(const AllocHandle& handle) const
	{
		const FreeListAllocation& allocation = m_static_gpu_memory_allocator.Get(handle);
		
		return allocation.offset;
	}

	size_t GPUMemoryRenderModule::GetDynamicGPUMemoryOffset(display::Device* device, void* allocation) const
	{
		//Calculate offsets
		uint8_t* dynamic_memory_base = m_dynamic_gpu_memory_base_ptr;
		uint8_t* allocation_uint8 = reinterpret_cast<uint8_t*>(allocation);

		assert(allocation_uint8 > dynamic_memory_base);
		assert((allocation_uint8 - dynamic_memory_base) < m_dynamic_gpu_memory_size);

		return allocation_uint8 - dynamic_memory_base;
	}

	display::WeakShaderResourceHandle GPUMemoryRenderModule::GetDynamicGPUMemoryResource()
	{
		return m_dynamic_gpu_memory_buffer;
	}

	void GPUMemoryRenderModule::ExecuteGPUCopy(uint64_t frame_index, display::Context* display_context)
	{
		std::vector<CopyDataCommand> copy_commands;

		m_copy_data_commands[frame_index % kMaxFrames].Visit([&](std::vector<CopyDataCommand>& data)
			{
				copy_commands.insert(copy_commands.end(), data.begin(), data.end());

				data.clear();
			});

		if (copy_commands.size() > 0)
		{
			//Calculate the size needed
			size_t number_of_float4_copies = 0;

			for (auto& copy_command : copy_commands)
			{
				number_of_float4_copies += (copy_command.size / 16);
			}

			//Send all the the data commands to the GPU, format int2 (offset source, offset destination)
			size_t offset = m_dynamic_gpu_memory_allocator.Alloc(number_of_float4_copies * sizeof(uint32_t) * 2, frame_index);
			uint32_t* gpu_data = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(display_context->GetDevice(), m_dynamic_gpu_memory_buffer)) + offset);

			//Send it to the GPU, move from bytes to int4
			for (auto& copy_command : copy_commands)
			{
				const uint32_t count = copy_command.size / 16;

				for (uint32_t i = 0; i < count; ++i)
				{
					*gpu_data = i + (copy_command.source_offset / 16);
					gpu_data++;
					*gpu_data = i + (copy_command.dest_offset / 16);
					gpu_data++;
				}
			}

			//Execute copy

			//Set root signature
			display_context->SetRootSignature(display::Pipe::Compute, m_copy_data_compute_root_signature);

			//Set parameters
			uint32_t parameters[2];
			//Source offset
			parameters[0] = static_cast<uint32_t>(offset / 16);
			//Number of copies
			parameters[1] = static_cast<uint32_t>(number_of_float4_copies);

			display_context->SetUnorderedAccessBuffer(display::Pipe::Compute, 0, m_static_gpu_memory_buffer);
			display_context->SetShaderResource(display::Pipe::Compute, 1, m_dynamic_gpu_memory_buffer);
			display_context->SetConstants(display::Pipe::Compute, 2, parameters, 2);

			//Set pipeline
			display_context->SetPipelineState(m_copy_data_compute_pipeline_state);

			//Execute
			display::ExecuteComputeDesc desc;
			desc.group_count_y = desc.group_count_z = 1;
			desc.group_count_x = static_cast<uint32_t>(((number_of_float4_copies - 1) / 64) + 1);
			display_context->ExecuteCompute(desc);
		}
	}
	void SyncStaticGPUMemoryPass::Render(RenderContext& render_context) const
	{
		if (m_gpu_memory_render_module == nullptr)
		{
			m_gpu_memory_render_module = render::GetModule<GPUMemoryRenderModule>(render_context.GetRenderSystem(), "GPUMemory"_sh32);
		}

		m_gpu_memory_render_module->ExecuteGPUCopy(render::GetRenderFrameIndex(render_context.GetRenderSystem()), render_context.GetContext());
	}
}
