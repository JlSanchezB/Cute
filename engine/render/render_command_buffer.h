//////////////////////////////////////////////////////////////////////////
// Cute engine - Virtual command buffer that captures render commands from the game to the render
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_COMMAND_BUFFER_h
#define RENDER_COMMAND_BUFFER_h

#include <display/display.h>
#include <core/command_buffer.h>

namespace render
{
	class CommandBuffer : public core::CommandBuffer<uint8_t>
	{
	public:
		using CommandOffset = uint32_t;
		const static CommandOffset InvalidCommandOffset = static_cast<uint32_t>(-1);

		//Starts a capture of a command buffer
		CommandOffset Open();

		//Close capture
		void Close();

		//Execute commands in this offset
		//Returns the offset of the next command in the render command, InvalidCommandOffset if it is the last
		CommandOffset Execute(display::Context& context, CommandOffset command_offset = 0);

		//Set pipeline state
		void SetPipelineState(const display::WeakPipelineStateHandle& pipeline_state);

		//Set Vertex buffers
		void SetVertexBuffers(uint8_t start_slot_index, uint8_t num_vertex_buffers, display::WeakVertexBufferHandle* vertex_buffers);

		//Set shader resource as a vertex buffer
		void SetShaderResourceAsVertexBuffer(uint8_t start_slot_index, const display::WeakShaderResourceHandle& handle, const display::SetShaderResourceAsVertexBufferDesc& desc);

		//Set Index Buffer
		void SetIndexBuffer(const display::WeakIndexBufferHandle& index_buffer);

		//Set constant buffer
		void SetConstantBuffer(const display::Pipe& pipe, uint8_t root_parameter, const display::WeakConstantBufferHandle& constant_buffer);

		//Set unordered access buffer
		void SetUnorderedAccessBuffer(const display::Pipe& pipe, uint8_t root_parameter, const display::WeakUnorderedAccessBufferHandle& unordered_access_buffer);

		//Set shader resource
		void SetShaderResource(const display::Pipe& pipe, uint8_t root_parameter, const display::Context::ShaderResourceSet& shader_resource);

		//Set descriptor table
		void SetDescriptorTable(const display::Pipe& pipe, uint8_t root_parameter, const display::WeakDescriptorTableHandle& descriptor_table);

		//Set descriptor table
		void SetDescriptorTable(const display::Pipe& pipe, uint8_t root_parameter, const display::WeakSamplerDescriptorTableHandle& sampler_descriptor_table);

		//Draw
		void Draw(const display::DrawDesc& draw_desc);

		//Draw Indexed
		void DrawIndexed(const display::DrawIndexedDesc& draw_desc);

		//Draw Indexed Instanced
		void DrawIndexedInstanced(const display::DrawIndexedInstancedDesc& draw_desc);

		//Execute compute
		void ExecuteCompute(const display::ExecuteComputeDesc& execute_compute_desc);

		//Upload resource
		//If data is null, returns an internal buffer where the memory can get copied
		void* UploadResourceBuffer(const display::UpdatableResourceHandle& handle, const void* data, size_t size);
	};
}

#endif //RENDER_COMMAND_BUFFER_h
