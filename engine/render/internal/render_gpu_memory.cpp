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
		static_buffer_desc.element_size = 16; //float4
		static_buffer_desc.element_count = static_gpu_memory_size / 16;
		m_static_gpu_memory_buffer = display::CreateUnorderedAccessBuffer(device, static_buffer_desc, "StaticGpuMemoryBuffer");

		//Init static allocator
		m_static_gpu_memory_allocator.Init(static_gpu_memory_size);

		//Init dynamic buffer
		display::UnorderedAccessBufferDesc dynamic_buffer_desc;
		dynamic_buffer_desc.element_size = 16; //float4
		dynamic_buffer_desc.element_count = dynamic_gpu_memory_size / 16;
		m_dynamic_gpu_memory_buffer = display::CreateUnorderedAccessBuffer(device, dynamic_buffer_desc, "DynamicGpuMemoryBuffer");

		//Init dynamic allocator
		m_dynamic_gpu_memory_allocator.Init(dynamic_gpu_memory_size, dynamic_gpu_memory_segment_size);
	}

	void RenderGPUMemory::Destroy(display::Device* device)
	{
		display::DestroyUnorderedAccessBuffer(device, m_static_gpu_memory_buffer);
		display::DestroyUnorderedAccessBuffer(device, m_dynamic_gpu_memory_buffer);
	}
}
