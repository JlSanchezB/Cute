//////////////////////////////////////////////////////////////////////////
// Cute engine - Virtual command buffer that captures render commands from the game to the render
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_COMMAND_BUFFER_h
#define RENDER_COMMAND_BUFFER_h

#include <display/display.h>
#include <vector>

namespace render
{
	class CommandBuffer
	{
	public:
		using CommandOffset = uint32_t;
		using Command = uint8_t;

		//Starts a capture of a command buffer
		CommandOffset Begin()
		{
			return static_cast<CommandOffset>(m_commands.size());
		}

		//Close capture
		void Close()
		{
			//Zero is always to close commands
			push_command(0);
		}
/*
		//Set pipeline state
		void SetPipelineState(const display::WeakPipelineStateHandle& pipeline_state);

		//Set Vertex buffers
		void SetVertexBuffers(uint8_t start_slot_index, uint8_t num_vertex_buffers, display::WeakVertexBufferHandle* vertex_buffers);

		//Set Index Buffer
		void SetIndexBuffer(const display::WeakIndexBufferHandle& index_buffer);

		//Set constants
		void SetConstants(const display::Pipe& pipe, uint8_t root_parameter, const void* data, size_t size);

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
		*/

		//Push command
		void push_command(const Command& command);

		//Push command data
		template<typename DATA>
		void push_data(const DATA& data);

		//Get command
		Command get_command(size_t& offset);

		//Get command data
		template<typename DATA>
		DATA& get_data(size_t& offset);

	private:

		//Returns the offset needed for alignment for ptr
		inline size_t calculate_alignment(size_t alignment, size_t offset);

		//List of commands in the command buffer
		std::vector<Command> m_commands;
		//Data associated to each command
		std::vector<uint8_t> m_command_data;
	};

	//Push command
	inline void CommandBuffer::push_command(const Command & command)
	{
		m_commands.push_back(command);
	}

	//Get command
	inline CommandBuffer::Command CommandBuffer::get_command(size_t & offset)
	{
		return m_commands[offset++];
	}

	//Returns the offset needed for alignment for ptr
	inline size_t CommandBuffer::calculate_alignment(size_t alignment, size_t offset)
	{
		size_t bias = offset % alignment;
		return (bias == 0) ? 0 : (alignment - bias);
	}

	//Push command data
	template<typename DATA>
	inline void CommandBuffer::push_data(const DATA & data)
	{
		size_t alignment_offset = calculate_alignment(alignof(DATA), sizeof(DATA), m_command_data.size());

		//Reserve memory as needed
		m_command_data.resize(alignment_offset + sizeof(DATA));

		//Copy data
		memcpy(&m_command_data[m_command_data.size() - sizeof(DATA)], &data, sizeof(DATA));
	}

	//Get command data
	template<typename DATA>
	inline DATA & CommandBuffer::get_data(size_t & offset)
	{
		size_t alignment_offset = calculate_alignment(alignof(DATA), sizeof(DATA), m_command_data.size());
		//Move offset
		offset += alignment_offset + sizeof(DATA);
		//Return data
		return *reinterpret_cast<DATA*>(&m_command_data[m_command_data.size() - sizeof(DATA)]);
	}
}

#endif //RENDER_COMMAND_BUFFER_h
