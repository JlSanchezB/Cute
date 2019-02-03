//////////////////////////////////////////////////////////////////////////
// Cute engine - Virtual command buffer that captures commands with data
//////////////////////////////////////////////////////////////////////////
#ifndef COMMAND_BUFFER_h
#define COMMAND_BUFFER_h

#include <vector>

namespace core
{
	template<typename COMMAND_TYPE>
	class CommandBuffer
	{
	public:
		using Command = COMMAND_TYPE;

		//Push command
		void push_command(const Command& command);

		//Push command data
		template<typename DATA>
		void push_data(const DATA& data);

		//Push command data
		template<typename DATA>
		void push_data_array(const DATA* data, size_t num);

		//Get command
		Command get_command(size_t& offset);

		//Get command data
		template<typename DATA>
		DATA& get_data(size_t& offset);

		//Get command data array
		template<typename DATA>
		DATA* get_data_array(size_t& offset, size_t num);

		//Get current offset of commands
		size_t get_current_command_position() const
		{
			return m_commands.size();
		}

		//Get current offset of command data
		size_t get_current_command_data_position() const
		{
			return m_command_data.size();
		}

	private:

		//Returns the offset needed for alignment for ptr
		inline size_t calculate_alignment(size_t alignment, size_t offset);

		//List of commands in the command buffer
		std::vector<Command> m_commands;
		//Data associated to each command
		std::vector<std::byte> m_command_data;
	};

	//Push command
	template<typename COMMAND_TYPE>
	inline void CommandBuffer<COMMAND_TYPE>::push_command(const Command & command)
	{
		m_commands.push_back(command);
	}

	//Get command
	template<typename COMMAND_TYPE>
	inline typename CommandBuffer<COMMAND_TYPE>::Command CommandBuffer<COMMAND_TYPE>::get_command(size_t & offset)
	{
		return m_commands[offset++];
	}

	//Returns the offset needed for alignment for ptr
	template<typename COMMAND_TYPE>
	inline size_t CommandBuffer<COMMAND_TYPE>::calculate_alignment(size_t alignment, size_t offset)
	{
		size_t bias = offset % alignment;
		return (bias == 0) ? 0 : (alignment - bias);
	}

	//Push command data
	template<typename COMMAND_TYPE>
	template<typename DATA>
	inline void CommandBuffer<COMMAND_TYPE>::push_data(const DATA & data)
	{
		size_t alignment_offset = calculate_alignment(alignof(DATA), m_command_data.size());

		//Reserve memory as needed
		const size_t begin_offset = m_command_data.size() + alignment_offset;
		m_command_data.resize(alignment_offset + sizeof(DATA));

		//Copy data
		memcpy(&m_command_data[begin_offset], &data, sizeof(DATA));
	}

	//Push command data array
	template<typename COMMAND_TYPE>
	template<typename DATA>
	inline void CommandBuffer<COMMAND_TYPE>::push_data_array(const DATA * data, size_t num)
	{
		size_t alignment_offset = calculate_alignment(alignof(DATA), m_command_data.size());

		//Reserve memory as needed
		const size_t begin_offset = m_command_data.size() + alignment_offset;
		m_command_data.resize(alignment_offset + sizeof(DATA) * num);

		//Copy data
		memcpy(&m_command_data[begin_offset], data, sizeof(DATA) * num);
	}

	//Get command data
	template<typename COMMAND_TYPE>
	template<typename DATA>
	inline DATA & CommandBuffer<COMMAND_TYPE>::get_data(size_t & offset)
	{
		size_t alignment_offset = calculate_alignment(alignof(DATA), m_command_data.size());
		const size_t begin_offset = m_command_data.size() + alignment_offset;

		//Move offset
		offset += alignment_offset + sizeof(DATA);
		//Return data
		return *reinterpret_cast<DATA*>(&m_command_data[begin_offset]);
	}

	//Get command data array
	template<typename COMMAND_TYPE>
	template<typename DATA>
	inline DATA * CommandBuffer<COMMAND_TYPE>::get_data_array(size_t & offset, size_t num)
	{
		size_t alignment_offset = calculate_alignment(alignof(DATA), m_command_data.size());
		const size_t begin_offset = m_command_data.size() + alignment_offset;

		//Move offset
		offset += alignment_offset + sizeof(DATA) * num;
		//Return data
		return reinterpret_cast<DATA*>(&m_command_data[begin_offset]);
	}
}

#endif //COMMAND_BUFFER_h
