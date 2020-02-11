#include "render_gpu_memory.h"
#include "display/display.h"

namespace render
{

	void RenderGPUMemory::Init(display::Device* device, uint32_t static_gpu_memory_size, uint32_t dynamic_gpu_memory_size, uint32_t dynamic_gpu_memory_segment_size)
	{
		assert(static_gpu_memory_size % 16 == 0);
		assert(dynamic_gpu_memory_size % 16 == 0);

		//Init static buffer
		display::ShaderResourceDesc static_buffer_desc;
		static_buffer_desc.access = display::Access::Static;
		static_buffer_desc.type = display::ShaderResourceType::Buffer;
		static_buffer_desc.size = static_gpu_memory_size;
		static_buffer_desc.num_elements = static_gpu_memory_size / 16;
		static_buffer_desc.structure_stride = 16;

		m_static_gpu_memory_buffer = display::CreateShaderResource(device, static_buffer_desc, "StaticGpuMemoryBuffer");

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
	}

	void RenderGPUMemory::Destroy(display::Device* device)
	{
		display::DestroyShaderResource(device, m_static_gpu_memory_buffer);
		display::DestroyShaderResource(device, m_dynamic_gpu_memory_buffer);
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


		}
	}
}
