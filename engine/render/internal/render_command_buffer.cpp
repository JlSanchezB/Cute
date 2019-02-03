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
		SetUnorderedAccessBuffer,
		SetShaderResource,
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
	CommandBuffer::CommandOffset CommandBuffer::Begin()
	{
		//We need to save the location of the data offset
		//We are going to use the same command buffer, so we now that always the first 4 commands
		//if the command buffer commads represent the offset to the command data offset
		uint32_t command_data_offset = static_cast<uint32_t>(get_current_command_data_position());
		uint32_t command_offset = static_cast<uint32_t>(get_current_command_position());
		push_command(static_cast<uint8_t>(command_data_offset));
		push_command(static_cast<uint8_t>(command_data_offset << 8));
		push_command(static_cast<uint8_t>(command_data_offset << 16));
		push_command(static_cast<uint8_t>(command_data_offset << 24));
		
		return command_offset;
	}

	//Close capture
	void CommandBuffer::Close()
	{
		push_command(static_cast<uint8_t>(Commands::Close));
	}

	void CommandBuffer::Execute(display::Context & context, CommandOffset command_offset)
	{
		size_t offset = command_offset;

		//Data offset is coded in the first 4 commands
		size_t data_offset = get_command(offset);
		data_offset |= get_command(offset) << 8;
		data_offset |= get_command(offset) << 16;
		data_offset |= get_command(offset) << 24;

		//Go for all commands until the close commands and execute the correct render context calls
		Commands command = static_cast<Commands>(get_command(offset));

		while (command != Commands::Close)
		{
			switch (command)
			{
			case Commands::SetPipelineState:
				context.SetPipelineState(get_data<display::WeakPipelineStateHandle>(data_offset));
				break;
			case Commands::SetVertexBuffers:
			{
				const uint8_t start_slot_index = get_data<uint8_t>(data_offset);
				const uint8_t num_vertex_buffers = get_data<uint8_t>(data_offset);
				context.SetVertexBuffers(start_slot_index, num_vertex_buffers, get_data_array<display::WeakVertexBufferHandle>(data_offset, num_vertex_buffers));
			}
				break;
			case Commands::SetIndexBuffer:
				context.SetIndexBuffer(get_data<display::WeakIndexBufferHandle>(data_offset));
				break;
			case Commands::SetConstantBuffer:
				context.SetConstantBuffer(get_data<display::Pipe>(data_offset), get_data<uint8_t>(data_offset), get_data<display::WeakConstantBufferHandle>(data_offset));
				break;
			case Commands::SetUnorderedAccessBuffer:
				context.SetUnorderedAccessBuffer(get_data<display::Pipe>(data_offset), get_data<uint8_t>(data_offset), get_data<display::WeakUnorderedAccessBufferHandle>(data_offset));
				break;
			case Commands::SetShaderResource:
				context.SetShaderResource(get_data<display::Pipe>(data_offset), get_data<uint8_t>(data_offset), get_data<display::Context::ShaderResourceSet>(data_offset));
				break;
			case Commands::SetDescriptorTable:
				context.SetDescriptorTable(get_data<display::Pipe>(data_offset), get_data<uint8_t>(data_offset), get_data<display::WeakDescriptorTableHandle>(data_offset));
				break;
			case Commands::SetSamplerDescriptorTable:
				context.SetDescriptorTable(get_data<display::Pipe>(data_offset), get_data<uint8_t>(data_offset), get_data<display::WeakSamplerDescriptorTableHandle>(data_offset));
				break;
			case Commands::Draw:
				context.Draw(get_data<display::DrawDesc>(data_offset));
				break;
			case Commands::DrawIndexed:
				context.DrawIndexed(get_data<display::DrawIndexedDesc>(data_offset));
				break;
			case Commands::DrawIndexedInstanced:
				context.DrawIndexedInstanced(get_data<display::DrawIndexedInstancedDesc>(data_offset));
				break;
			case Commands::ExecuteCompute:
				context.ExecuteCompute(get_data<display::ExecuteComputeDesc>(data_offset));
				break;
			case Commands::UploadResourceBuffer:
			{
				size_t size = get_data<size_t>(data_offset);
				uint8_t* data = get_data_array<uint8_t>(data_offset, size);
				display::UpdateResourceBuffer(context.GetDevice(), get_data<display::UpdatableResourceHandle>(data_offset), data, size);
			}
			default:
				//Command non know
				throw std::runtime_error("Command in the command buffer is not known");
				break;
			}

			//Next command
			command = static_cast<Commands>(get_command(offset));
		}
	}

	void CommandBuffer::SetPipelineState(const display::WeakPipelineStateHandle & pipeline_state)
	{
		push_command(static_cast<uint8_t>(Commands::SetPipelineState));
		push_data(pipeline_state);
	}

	void CommandBuffer::SetVertexBuffers(uint8_t start_slot_index, uint8_t num_vertex_buffers, display::WeakVertexBufferHandle * vertex_buffers)
	{
		push_command(static_cast<uint8_t>(Commands::SetVertexBuffers));
		push_data(num_vertex_buffers);
		push_data_array(vertex_buffers, num_vertex_buffers);
	}

	void CommandBuffer::SetIndexBuffer(const display::WeakIndexBufferHandle & index_buffer)
	{
		push_command(static_cast<uint8_t>(Commands::SetIndexBuffer));
		push_data(index_buffer);
	}

	void CommandBuffer::SetConstantBuffer(const display::Pipe & pipe, uint8_t root_parameter, const display::WeakConstantBufferHandle & constant_buffer)
	{
		push_command(static_cast<uint8_t>(Commands::SetConstantBuffer));
		push_data(pipe);
		push_data(root_parameter);
		push_data(constant_buffer);
	}

	void CommandBuffer::SetUnorderedAccessBuffer(const display::Pipe & pipe, uint8_t root_parameter, const display::WeakUnorderedAccessBufferHandle & unordered_access_buffer)
	{
		push_command(static_cast<uint8_t>(Commands::SetUnorderedAccessBuffer));
		push_data(pipe);
		push_data(root_parameter);
		push_data(unordered_access_buffer);
	}

	void CommandBuffer::SetShaderResource(const display::Pipe & pipe, uint8_t root_parameter, const display::Context::ShaderResourceSet & shader_resource)
	{
		push_command(static_cast<uint8_t>(Commands::SetShaderResource));
		push_data(pipe);
		push_data(root_parameter);
		push_data(shader_resource);
	}

	void CommandBuffer::SetDescriptorTable(const display::Pipe & pipe, uint8_t root_parameter, const display::WeakDescriptorTableHandle & descriptor_table)
	{
		push_command(static_cast<uint8_t>(Commands::SetDescriptorTable));
		push_data(pipe);
		push_data(root_parameter);
		push_data(descriptor_table);
	}

	void CommandBuffer::SetDescriptorTable(const display::Pipe & pipe, uint8_t root_parameter, const display::WeakSamplerDescriptorTableHandle & sampler_descriptor_table)
	{
		push_command(static_cast<uint8_t>(Commands::SetSamplerDescriptorTable));
		push_data(pipe);
		push_data(root_parameter);
		push_data(sampler_descriptor_table);
	}

	void CommandBuffer::Draw(const display::DrawDesc & draw_desc)
	{
		push_command(static_cast<uint8_t>(Commands::Draw));
		push_data(draw_desc);
	}

	void CommandBuffer::DrawIndexed(const display::DrawIndexedDesc & draw_desc)
	{
		push_command(static_cast<uint8_t>(Commands::DrawIndexed));
		push_data(draw_desc);
	}

	void CommandBuffer::DrawIndexedInstanced(const display::DrawIndexedInstancedDesc & draw_desc)
	{
		push_command(static_cast<uint8_t>(Commands::DrawIndexedInstanced));
		push_data(draw_desc);
	}

	void CommandBuffer::ExecuteCompute(const display::ExecuteComputeDesc & execute_compute_desc)
	{
		push_command(static_cast<uint8_t>(Commands::ExecuteCompute));
		push_data(execute_compute_desc);
	}

	void CommandBuffer::UploadResourceBuffer(const display::UpdatableResourceHandle & handle, const void * data, size_t size)
	{
		push_command(static_cast<uint8_t>(Commands::UploadResourceBuffer));
		push_data(size);
		push_data_array(reinterpret_cast<const uint8_t*>(data), size);
		push_data(handle);
	}

}