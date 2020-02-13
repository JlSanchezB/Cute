#include "render_gpu_memory.h"
#include "display/display.h"

namespace render
{

	void RenderGPUMemory::Init(display::Device* device, uint32_t static_gpu_memory_size, uint32_t dynamic_gpu_memory_size, uint32_t dynamic_gpu_memory_segment_size)
	{
		assert(static_gpu_memory_size % 16 == 0);
		assert(dynamic_gpu_memory_size % 16 == 0);

		//Init static buffer
		display::UnorderedAccessBufferDesc static_buffer_desc;
		static_buffer_desc.element_size = 16;
		static_buffer_desc.element_count = static_gpu_memory_size / 16;

		m_static_gpu_memory_buffer = display::CreateUnorderedAccessBuffer(device, static_buffer_desc, "StaticGpuMemoryBuffer");

		//Init static allocator
		m_static_gpu_memory_allocator.Init(static_gpu_memory_size);

		//Init dynamic buffer
		display::ShaderResourceDesc dynamic_buffer_desc;
		dynamic_buffer_desc.access = display::Access::Upload;
		dynamic_buffer_desc.type = display::ShaderResourceType::Buffer;
		dynamic_buffer_desc.size = dynamic_gpu_memory_size;
		dynamic_buffer_desc.num_elements = dynamic_gpu_memory_size / 16;
		dynamic_buffer_desc.structure_stride = 16;
		
		m_dynamic_gpu_memory_buffer = display::CreateShaderResource(device, dynamic_buffer_desc, "DynamicGpuMemoryBuffer");

		//Init dynamic allocator
		m_dynamic_gpu_memory_allocator.Init(dynamic_gpu_memory_size, dynamic_gpu_memory_segment_size);


		//Copy data compute
		{
			//Create compute root signature
			display::RootSignatureDesc root_signature_desc;
			root_signature_desc.num_root_parameters = 3;
			root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::UnorderAccessBuffer;
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
							uint4 copy_data = source_buffer[parameters.x + thread.x];\
							for (uint i = 0; i < copy_data.z; ++i)\
							{\
								destination_buffer[copy_data.y + i] = source_buffer[copy_data.x + i];\
							}\
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
		}
	}

	void RenderGPUMemory::Destroy(display::Device* device)
	{
		display::DestroyHandle(device, m_static_gpu_memory_buffer);
		display::DestroyHandle(device, m_dynamic_gpu_memory_buffer);
		display::DestroyHandle(device, m_copy_data_compute_root_signature);
		display::DestroyHandle(device, m_copy_data_compute_pipeline_state);
	}
	void RenderGPUMemory::Sync(uint64_t cpu_frame_index, uint64_t freed_frame_index)
	{
		m_dynamic_gpu_memory_allocator.Sync(cpu_frame_index, freed_frame_index);
		m_static_gpu_memory_allocator.Sync(freed_frame_index);
	}

	void RenderGPUMemory::ExecuteGPUCopy(uint64_t frame_index, display::Context* display_context)
	{
		std::vector<CopyDataCommand> copy_commands;

		m_copy_data_commands[frame_index].Visit([&](std::vector<CopyDataCommand>& data)
			{
				copy_commands.insert(copy_commands.end(), data.begin(), data.end());
			});

		if (copy_commands.size() > 0)
		{
			//Sort all commands by size
			//We want the waves to have the most similar sizes posible, so we don't diverge a lot
			std::sort(copy_commands.begin(), copy_commands.end(), [](const CopyDataCommand& a, const CopyDataCommand& b)
				{
					return a.size < b.size;
				});

			//Send all the the data commands to the GPU, format int4
			size_t offset = m_dynamic_gpu_memory_allocator.Alloc(copy_commands.size() * sizeof(uint32_t) * 4, frame_index);
			uint32_t* gpu_data = reinterpret_cast<uint32_t*>(display::GetResourceMemoryBuffer(display_context->GetDevice(), m_dynamic_gpu_memory_buffer)) + offset;

			for (auto& copy_command : copy_commands)
			{
				*gpu_data = copy_command.source_offset;
				gpu_data++;
				*gpu_data = copy_command.dest_offset;
				gpu_data++;
				*gpu_data = copy_command.size;
				gpu_data++;
				//Add padding for int4
				gpu_data++;
			}

			//Resource barrier to writable uav
			display_context->AddResourceBarriers({ display::ResourceBarrier(m_static_gpu_memory_buffer, display::TranstitionState::AllShaderResource, display::TranstitionState::UnorderedAccess) });

			//Execute copy

			//Resource barrier to shader resource
			display_context->AddResourceBarriers({ display::ResourceBarrier(m_static_gpu_memory_buffer, display::TranstitionState::UnorderedAccess, display::TranstitionState::AllShaderResource) });
		}
	}
}
