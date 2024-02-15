#include "render_module_gpu_memory.h"
#include "display/display.h"
#include "render/render_resource.h"
#include "render/render_helper.h"
#include <core/profile.h>
#include <ext/imgui/imgui.h>
#include <helpers/imgui_helper.h>

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
		display::BufferDesc static_buffer_desc = display::BufferDesc::CreateRawAccessBuffer(display::Access::Static, m_static_gpu_memory_size, true);
		m_static_gpu_memory_buffer = display::CreateBuffer(device, static_buffer_desc, "StaticGpuMemoryBuffer");
		
		//Init static allocator
		m_static_gpu_memory_allocator.Init(m_static_gpu_memory_size);

		//Init dynamic buffer

		display::BufferDesc dynamic_buffer_desc = display::BufferDesc::CreateRawAccessBuffer(display::Access::Upload, m_dynamic_gpu_memory_size);
		m_dynamic_gpu_memory_buffer = display::CreateBuffer(device, dynamic_buffer_desc, "DynamicGpuMemoryBuffer");
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
				"RWByteAddressBuffer destination_buffer : register(u0); \n\
				ByteAddressBuffer source_buffer : register(t0); \n\
				uint4 parameters : register(b0); \n\
				\n\
				[numthreads(64, 1, 1)] \n\
				void copy_data(uint3 thread : SV_DispatchThreadID) \n\
				{ \n\
					if (thread.x < parameters.y) \n\
					{ \n\
						uint2 copy_command = source_buffer.Load2(parameters.x + thread.x * 2 * 4);\n\
						destination_buffer.Store4(copy_command.y, source_buffer.Load4(copy_command.x));\n\
					}\n\
				};";

			//Create pipeline state
			display::ComputePipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_copy_data_compute_root_signature;

			pipeline_state_desc.compute_shader.name = "CopyDataCompute";
			pipeline_state_desc.compute_shader.shader_code = shader_code;
			pipeline_state_desc.compute_shader.entry_point = "copy_data";
			pipeline_state_desc.compute_shader.target = "cs_6_0";


			//Create
			m_copy_data_compute_pipeline_state = display::CreateComputePipelineState(device, pipeline_state_desc, "Copy Data Compute");

			//Register resources
			AddGameResource(system, "StaticGPUMemoryBuffer"_sh32, CreateResourceFromHandle<render::BufferResource>(display::WeakBufferHandle(m_static_gpu_memory_buffer)));
			AddGameResource(system, "DynamicGPUMemoryBuffer"_sh32, CreateResourceFromHandle<render::BufferResource>(display::WeakBufferHandle(m_dynamic_gpu_memory_buffer)));


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

	void GPUMemoryRenderModule::DisplayImguiStats()
	{
		char buffer[1024];
		char buffer2[1024];

		helpers::FormatMemory(buffer, 1024, m_static_total_memory_allocated.load());
		helpers::FormatMemory(buffer2, 1024, m_static_gpu_memory_size);
		ImGui::Text("Static total memory allocated (%s/%s)", buffer, buffer2);
		helpers::FormatMemory(buffer, 1024, m_static_frame_memory_updated.load());
		ImGui::Text("Static frame memory updated (%s)", buffer);
		helpers::FormatMemory(buffer, 1024, m_dynamic_frame_memory_allocated.load());
		ImGui::Text("Dynamic frame memory allocated (%s)", buffer);
		ImGui::Text("Static frame allocations (%zu)", m_static_frame_allocations.load());
		ImGui::Text("Static frame deallocations (%zu)", m_static_frame_deallocations.load());
		ImGui::Text("Dynamic frame allocations (%zu)", m_dynamic_frame_allocations.load());
		ImGui::Text("Num frame render commands (%zu)", m_num_frame_render_commands);
		ImGui::Text("Num frame 16bytes copies (%zu)", m_num_frame_16bytes_copies);

		SegmentAllocator::Stats stats;
		m_dynamic_gpu_memory_allocator.CollectStats(stats);
		helpers::FormatMemory(buffer, 1024, stats.m_memory_alive);
		helpers::FormatMemory(buffer2, 1024, m_dynamic_gpu_memory_size);
		ImGui::Text("Dynamic memory alive (needed by the GPU) (%s/%s)", buffer, buffer2);

		m_dynamic_frame_memory_allocated = 0;
		m_static_frame_memory_updated = 0;
		m_static_frame_allocations = 0;
		m_dynamic_frame_allocations = 0;
		m_static_frame_deallocations = 0;
		m_num_frame_render_commands = 0;
		m_num_frame_16bytes_copies = 0;
	}

	void* GPUMemoryRenderModule::AllocDynamicGPUMemory(display::Device* device, const size_t size, const uint64_t frame_index)
	{
		assert(size > 0);
		assert(size % 16 == 0);

		size_t offset = m_dynamic_gpu_memory_allocator.Alloc(size, frame_index);

		m_dynamic_frame_memory_allocated += size;
		m_dynamic_frame_allocations++;

		//Return the memory address inside the resource
		return reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(device, m_dynamic_gpu_memory_buffer)) + offset;
	}

	void* GPUMemoryRenderModule::AllocDynamicSegmentGPUMemory(display::Device* device, const uint64_t frame_index)
	{
		size_t segment_index = m_dynamic_gpu_memory_allocator.AllocFullSegment(frame_index);

		m_dynamic_frame_memory_allocated += m_dynamic_gpu_memory_segment_size;
		m_dynamic_frame_allocations++;

		//Return the memory address inside the resource
		return reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(device, m_dynamic_gpu_memory_buffer)) + segment_index * m_dynamic_gpu_memory_segment_size;
	}

	AllocHandle GPUMemoryRenderModule::AllocStaticGPUMemory(display::Device* device, const size_t size, const void* data, const uint64_t frame_index)
	{
		assert(size > 0);
		assert(size % 16 == 0);

		AllocHandle handle = m_static_gpu_memory_allocator.Alloc(size);

		m_static_total_memory_allocated += size;
		m_static_frame_allocations++;

		if (data)
		{
			UpdateStaticGPUMemory(device, handle, data, size, frame_index);
		}

		return std::move(handle);
	}

	void GPUMemoryRenderModule::DeallocStaticGPUMemory(display::Device* device, AllocHandle& handle, const uint64_t frame_index)
	{
		m_static_total_memory_allocated -= m_static_gpu_memory_allocator.Get(handle).size;
		m_static_frame_deallocations++;

		m_static_gpu_memory_allocator.Dealloc(handle, frame_index);
	}

	void GPUMemoryRenderModule::UpdateStaticGPUMemory(display::Device* device, const AllocHandle& handle, const void* data, const size_t size, const uint64_t frame_index, size_t destination_offset)
	{
		assert(size > 0);
		assert(size % 16 == 0);

		m_static_frame_memory_updated += size;

		//We need so spit it using the segment size
		size_t num_segments = 1 + ((size - 1) / m_dynamic_gpu_memory_segment_size);

		const std::byte* data_byte = reinterpret_cast<const std::byte*>(data);

		for (size_t i = 0; i < num_segments; ++i)
		{
			size_t begin_data = i * m_dynamic_gpu_memory_segment_size;
			size_t end_data = std::min((i + 1) * m_dynamic_gpu_memory_segment_size, size);
			//Data gets copied in the dynamic gpu memory
			void* gpu_memory = AllocDynamicGPUMemory(device, end_data - begin_data, frame_index);
			memcpy(gpu_memory, &data_byte[begin_data], end_data - begin_data);

			//Calculate offsets
			uint8_t* dynamic_memory_base = reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(device, m_dynamic_gpu_memory_buffer));

			uint32_t source_offset = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(gpu_memory) - dynamic_memory_base);
			const auto& destination_allocation = m_static_gpu_memory_allocator.Get(handle);

			//Add copy command
			AddCopyDataCommand(frame_index, source_offset, static_cast<uint32_t>(destination_allocation.offset + destination_offset + begin_data), static_cast<uint32_t>(end_data - begin_data));
		}
	}

	display::WeakBufferHandle GPUMemoryRenderModule::GetStaticGPUMemoryResource()
	{
		return m_static_gpu_memory_buffer;
	}

	size_t GPUMemoryRenderModule::GetStaticGPUMemoryOffset(const AllocHandle& handle) const
	{
		const auto& allocation = m_static_gpu_memory_allocator.Get(handle);
		
		return allocation.offset;
	}

	size_t GPUMemoryRenderModule::GetDynamicGPUMemoryOffset(display::Device* device, void* allocation) const
	{
		//Calculate offsets
		uint8_t* dynamic_memory_base = m_dynamic_gpu_memory_base_ptr;
		uint8_t* allocation_uint8 = reinterpret_cast<uint8_t*>(allocation);

		assert(allocation_uint8 >= dynamic_memory_base);
		assert((allocation_uint8 - dynamic_memory_base) < m_dynamic_gpu_memory_size);

		return allocation_uint8 - dynamic_memory_base;
	}

	display::WeakBufferHandle GPUMemoryRenderModule::GetDynamicGPUMemoryResource()
	{
		return m_dynamic_gpu_memory_buffer;
	}

	void GPUMemoryRenderModule::ExecuteGPUCopy(uint64_t frame_index, display::Context* display_context)
	{
		PROFILE_SCOPE("Render", kRenderProfileColour, "ExecuteGPUCopy");

		std::vector<CopyDataCommand> copy_commands;

		m_copy_data_commands[frame_index % kMaxFrames].Visit([&](std::vector<CopyDataCommand>& data)
			{
				copy_commands.insert(copy_commands.end(), data.begin(), data.end());

				data.clear();
			});

		m_num_frame_render_commands = copy_commands.size();

		if (copy_commands.size() > 0)
		{
			size_t begin_command = 0;

			//We only can copy the max of a segment of the dynamic gpu memory
			size_t max_float4_by_dispath = m_dynamic_gpu_memory_segment_size / (sizeof(uint32_t) * 2);

			bool final_pass = true;
			do
			{
				//Calculate the size needed and if fits in the segment memory size
				size_t number_of_float4_copies = 0;

				final_pass = true;
				size_t last_command = copy_commands.size();
				for (size_t i = begin_command; i < last_command; ++i)
				{
					auto& copy_command = copy_commands[i];
					assert(copy_command.size % 16 == 0);
					if ((number_of_float4_copies + (copy_command.size / 16)) > max_float4_by_dispath)
					{
						//It can not process more
						final_pass = false;
						last_command = i;
						break; //Out
					}
					number_of_float4_copies += (copy_command.size / 16);
				}

				//Send all the the data commands to the GPU, format int2 (offset source, offset destination)
				size_t offset = m_dynamic_gpu_memory_allocator.Alloc(number_of_float4_copies * sizeof(uint32_t) * 2, frame_index);
				uint32_t* gpu_data = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(display::GetResourceMemoryBuffer(display_context->GetDevice(), m_dynamic_gpu_memory_buffer)) + offset);

				//Send it to the GPU, move from bytes to int4
				for (size_t i = begin_command; i < last_command; ++i)
				{
					auto& copy_command = copy_commands[i];

					const uint32_t count = copy_command.size / 16;

					for (uint32_t i = 0; i < count; ++i)
					{
						*gpu_data = copy_command.source_offset + i * 16;
						gpu_data++;
						*gpu_data = copy_command.dest_offset + i * 16;
						gpu_data++;
					}
				}

				//Execute copy

				//Set root signature
				display_context->SetRootSignature(display::Pipe::Compute, m_copy_data_compute_root_signature);

				//Set parameters
				uint32_t parameters[2];
				//Source offset
				parameters[0] = static_cast<uint32_t>(offset);
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

				m_num_frame_16bytes_copies += number_of_float4_copies;

				begin_command = last_command;
			}
			while (!final_pass);
		}
	}
	void SyncStaticGPUMemoryPass::Render(RenderContext& render_context) const
	{
		if (m_gpu_memory_render_module == nullptr)
		{
			m_gpu_memory_render_module = render::GetModule<GPUMemoryRenderModule>(render_context.GetRenderSystem());
		}

		m_gpu_memory_render_module->ExecuteGPUCopy(render::GetRenderFrameIndex(render_context.GetRenderSystem()), render_context.GetContext());
	}
}
