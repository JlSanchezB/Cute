//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for the GPU memory model used for sending data from CPU to GPU
//////////////////////////////////////////////////////////////////////////

#ifndef RENDER_GPU_MEMORY_H_
#define RENDER_GPU_MEMORY_H_

#include <render/render_segment_allocator.h>
#include <display/display_handle.h>

namespace render
{
	//Instance buffer is a GPU memory buffer that can be updated using a copy commands
	//Copy commands will go from CPU to GPU and update the instance buffer at the correct moment
	class RenderInstanceBuffer
	{
	public:

	private:
		//Instance buffer resource in the GPU
		display::UnorderedAccessBufferHandle m_instance_buffer;

		//Commands ring buffer allocator
		SegmentAllocator m_segment_resource_allocator;

	};
}

#endif //RENDER_GPU_MEMORY_H_