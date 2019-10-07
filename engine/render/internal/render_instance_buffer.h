//////////////////////////////////////////////////////////////////////////
// Cute engine - Instance buffer implementation for the instance buffer
//////////////////////////////////////////////////////////////////////////

#ifndef RENDER_INSTANCE_BUFFER_H_
#define RENDER_INSTANCE_BUFFER_H_

#include <render/render_sub_allocators.h>
#include <display/display_handle.h>

namespace render
{
	//Instance buffer is a GPU memory buffer that can be updated using a copy commands
	//Copy commands will go from CPU to GPU and update the instance buffer at the correct moment
	template <size_t NUM_COMMANDS_RESOURCES>
	class RenderInstanceBuffer
	{
	public:

	private:
		//Instance buffer resource in the GPU
		display::UnorderedAccessBufferHandle m_instance_buffer;

		//Commands ring buffer allocator
		SegmentAllocator<1024 * 1024, NUM_COMMANDS_RESOURCES> m_segment_resource_allocator;

		//Commands ring buffer resources
		std::array<display::UnorderedAccessBufferHandle, NUM_COMMANDS_RESOURCES> m_commands_resources;
	};
}

#endif //RENDER_INSTANCE_BUFFER_H_