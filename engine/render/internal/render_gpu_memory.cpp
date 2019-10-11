#include "render_gpu_memory.h"

namespace render
{

	void RenderGPUMemory::Init(size_t static_gpu_memory_size, size_t dynamic_gpu_memory_size, size_t dynamic_gpu_memory_segment_size)
	{
		//Init static buffer

		//Init static allocator
		m_static_gpu_memory_allocator.Init(static_gpu_memory_size);

		//Init dynamic buffer

		//Init dynamic allocator
		m_dynamic_gpu_memory_allocator.Init(dynamic_gpu_memory_size, dynamic_gpu_memory_segment_size);
	}

	void RenderGPUMemory::Destroy()
	{

	}
}
