//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for the GPU memory model used for sending data from CPU to GPU
//////////////////////////////////////////////////////////////////////////

#ifndef RENDER_GPU_MEMORY_H_
#define RENDER_GPU_MEMORY_H_

#include <render/render_segment_allocator.h>
#include <render/render_freelist_allocator.h>
#include <display/display_handle.h>

namespace render
{
	//Render GPU memory offers the user a gpu memory for uploading data from the cpu
	
	//Static data is GPU only memory, only need to be sended once and the user can allocate a piece of it
	//Static data must keep static between frames, but still can be modified.

	//Dynamic data only exist during the frame that is allocated
	//Users doesn't need to keep track of this memory, will be avaliable until the gpu uses it
	class RenderGPUMemory
	{
	public:
		void Init(size_t static_gpu_memory_size, size_t dynamic_gpu_memory_size, size_t dynamic_gpu_memory_segment_size);
		void Destroy();

		//Static buffer resource in the GPU
		display::UnorderedAccessBufferHandle m_static_gpu_memory_buffer;
		//Static gpu allocator
		FreeListAllocator m_static_gpu_memory_allocator;

		//Dynamic buffer resource in the GPU
		display::UnorderedAccessBufferHandle m_dynamic_gpu_memory_buffer;
		//Dynamic buffer resource in the GPU
		SegmentAllocator m_dynamic_gpu_memory_allocator;

	};
}

#endif //RENDER_GPU_MEMORY_H_