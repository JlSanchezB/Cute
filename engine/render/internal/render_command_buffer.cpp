#include <render/render_command_buffer.h>

namespace render
{
	enum class Commands : uint8_t
	{
		Close,
		SetPipelineState,
		SetVertexBuffers,
		SetIndexBuffer,
		SetConstantBuffer,
		SetDescriptorTable,
		SetSamplerDescriptorTable,
		Draw,
		DrawIndexed,
		DrawIndexedInstanced,
		ExecuteCompute,
		UploadResourceBuffer,
		Custom
	};

	//Starts a capture of a command buffer
	CommandBuffer::CommandOffset CommandBuffer::Open()
	{
		//We need to save the location of the data offset
		//We are going to use the same command buffer, so we now that always the first 4 commands
		//if the command buffer commads represent the offset to the command data offset
		uint32_t command_data_offset = static_cast<uint32_t>(GetCurrentCommandDataPosition());
		uint32_t command_offset = static_cast<uint32_t>(GetCurrentCommandPosition());
		PushCommand(static_cast<uint8_t>((command_data_offset & 0x000000FF)));
		PushCommand(static_cast<uint8_t>((command_data_offset & 0x0000FF00) >> 8));
		PushCommand(static_cast<uint8_t>((command_data_offset & 0x00FF0000) >> 16));
		PushCommand(static_cast<uint8_t>((command_data_offset & 0xFF000000) >> 24));
		
		return command_offset;
	}

	//Close capture
	void CommandBuffer::Close()
	{
		PushCommand(static_cast<uint8_t>(Commands::Close));
	}

	CommandBuffer::CommandOffset CommandBuffer::Execute(display::Context & context, CommandOffset command_offset)
	{
		if (command_offset >= GetCurrentCommandPosition())
		{
			return CommandOffset();
		}

		size_t offset = command_offset;

		//Data offset is coded in the first 4 commands
		size_t data_offset = GetCommand(offset);
		data_offset |= GetCommand(offset) << 8;
		data_offset |= GetCommand(offset) << 16;
		data_offset |= GetCommand(offset) << 24;

		//Go for all commands until the close commands and execute the correct render context calls
		Commands command = static_cast<Commands>(GetCommand(offset));

		while (command != Commands::Close)
		{
			switch (command)
			{
			case Commands::SetPipelineState:
				context.SetPipelineState(GetData<display::WeakPipelineStateHandle>(data_offset));
				break;
			case Commands::SetVertexBuffers:
			{
				const uint8_t start_slot_index = GetData<uint8_t>(data_offset);
				const uint8_t num_vertex_buffers = GetData<uint8_t>(data_offset);
				display::WeakBufferHandle handles[16];
				for (size_t i = 0; i < num_vertex_buffers; ++i)
				{
					handles[i] = GetData<display::WeakBufferHandle>(data_offset);
				}
				context.SetVertexBuffers(start_slot_index, num_vertex_buffers, handles);
			}
				break;
			case Commands::SetIndexBuffer:
				context.SetIndexBuffer(GetData<display::WeakBufferHandle>(data_offset));
				break;
			case Commands::SetConstantBuffer:
				context.SetConstantBuffer(GetData<display::Pipe>(data_offset), GetData<uint8_t>(data_offset), GetData<display::WeakBufferHandle>(data_offset));
				break;
			case Commands::SetDescriptorTable:
				context.SetDescriptorTable(GetData<display::Pipe>(data_offset), GetData<uint8_t>(data_offset), GetData<display::WeakDescriptorTableHandle>(data_offset));
				break;
			case Commands::SetSamplerDescriptorTable:
				context.SetDescriptorTable(GetData<display::Pipe>(data_offset), GetData<uint8_t>(data_offset), GetData<display::WeakSamplerDescriptorTableHandle>(data_offset));
				break;
			case Commands::Draw:
				context.Draw(GetData<display::DrawDesc>(data_offset));
				break;
			case Commands::DrawIndexed:
				context.DrawIndexed(GetData<display::DrawIndexedDesc>(data_offset));
				break;
			case Commands::DrawIndexedInstanced:
				context.DrawIndexedInstanced(GetData<display::DrawIndexedInstancedDesc>(data_offset));
				break;
			case Commands::ExecuteCompute:
				context.ExecuteCompute(GetData<display::ExecuteComputeDesc>(data_offset));
				break;
			case Commands::UploadResourceBuffer:
			{
				size_t size = GetData<size_t>(data_offset);
				const std::byte* buffer = GetBuffer(data_offset, size);
				
				display::UpdateResourceBuffer(context.GetDevice(), GetData<display::UpdatableResourceHandle>(data_offset), buffer, size);
			}
			break;
			default:
				//Command non know
				throw std::runtime_error("Command in the command buffer is not known");
				break;
			}

			//Next command
			command = static_cast<Commands>(GetCommand(offset));
		}

		if (offset == GetCurrentCommandPosition())
		{
			return CommandOffset();
		}
		else
		{
			return static_cast<CommandOffset>(static_cast<uint32_t>(offset));
		}
	}

	void CommandBuffer::SetPipelineState(const display::WeakPipelineStateHandle & pipeline_state)
	{
		PushCommand(static_cast<uint8_t>(Commands::SetPipelineState));
		PushData(pipeline_state);
	}

	void CommandBuffer::SetVertexBuffers(uint8_t start_slot_index, uint8_t num_vertex_buffers, display::WeakBufferHandle* vertex_buffers)
	{
		PushCommand(static_cast<uint8_t>(Commands::SetVertexBuffers));
		PushData(start_slot_index);
		PushData(num_vertex_buffers);
		PushDataArray(vertex_buffers, num_vertex_buffers);
	}

	void CommandBuffer::SetIndexBuffer(const display::WeakBufferHandle& index_buffer)
	{
		PushCommand(static_cast<uint8_t>(Commands::SetIndexBuffer));
		PushData(index_buffer);
	}

	void CommandBuffer::SetConstantBuffer(const display::Pipe & pipe, uint8_t root_parameter, const display::WeakBufferHandle& constant_buffer)
	{
		PushCommand(static_cast<uint8_t>(Commands::SetConstantBuffer));
		PushData(pipe);
		PushData(root_parameter);
		PushData(constant_buffer);
	}

	void CommandBuffer::SetDescriptorTable(const display::Pipe & pipe, uint8_t root_parameter, const display::WeakDescriptorTableHandle & descriptor_table)
	{
		PushCommand(static_cast<uint8_t>(Commands::SetDescriptorTable));
		PushData(pipe);
		PushData(root_parameter);
		PushData(descriptor_table);
	}

	void CommandBuffer::SetDescriptorTable(const display::Pipe & pipe, uint8_t root_parameter, const display::WeakSamplerDescriptorTableHandle & sampler_descriptor_table)
	{
		PushCommand(static_cast<uint8_t>(Commands::SetSamplerDescriptorTable));
		PushData(pipe);
		PushData(root_parameter);
		PushData(sampler_descriptor_table);
	}

	void CommandBuffer::Draw(const display::DrawDesc & draw_desc)
	{
		PushCommand(static_cast<uint8_t>(Commands::Draw));
		PushData(draw_desc);
	}

	void CommandBuffer::DrawIndexed(const display::DrawIndexedDesc & draw_desc)
	{
		PushCommand(static_cast<uint8_t>(Commands::DrawIndexed));
		PushData(draw_desc);
	}

	void CommandBuffer::DrawIndexedInstanced(const display::DrawIndexedInstancedDesc & draw_desc)
	{
		PushCommand(static_cast<uint8_t>(Commands::DrawIndexedInstanced));
		PushData(draw_desc);
	}

	void CommandBuffer::ExecuteCompute(const display::ExecuteComputeDesc & execute_compute_desc)
	{
		PushCommand(static_cast<uint8_t>(Commands::ExecuteCompute));
		PushData(execute_compute_desc);
	}

	void* CommandBuffer::UploadResourceBuffer(const display::UpdatableResourceHandle & handle, const void * data, size_t size)
	{
		PushCommand(static_cast<uint8_t>(Commands::UploadResourceBuffer));
		PushData(size);
		void* data_buffer_inside_command_buffer = PushBuffer(reinterpret_cast<const std::byte*>(data), size);
		PushData(handle);

		return data_buffer_inside_command_buffer;
	}

}