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

		//Clear the command buffer, it will not deallocate memory, just clear it
		void Reset();

		//Push command
		void PushCommand(const Command& command);

		//Push command data
		template<typename DATA>
		void PushData(const DATA& data);

		//Push command data
		template<typename DATA>
		void PushDataArray(const DATA* data, size_t num);

		//Get command
		Command GetCommand(size_t& offset);

		//Get command data
		template<typename DATA>
		DATA& GetData(size_t& offset);

		//Get command data array
		template<typename DATA>
		DATA* GetDataArray(size_t& offset, size_t num);

		//Get current offset of commands
		size_t GetCurrentCommandPosition() const
		{
			return m_commands.size();
		}

		//Get current offset of command data
		size_t GetCurrentCommandDataPosition() const
		{
			return m_command_data.size();
		}

	private:

		//Returns the offset needed for alignment for ptr
		inline size_t CalculateAlignment(size_t alignment, size_t offset);

		//List of commands in the command buffer
		std::vector<Command> m_commands;
		//Data associated to each command
		std::vector<std::byte> m_command_data;
	};

	template<typename COMMAND_TYPE>
	inline void CommandBuffer<COMMAND_TYPE>::Reset()
	{
		m_commands.clear();
		m_command_data.clear();
	}

	//Push command
	template<typename COMMAND_TYPE>
	inline void CommandBuffer<COMMAND_TYPE>::PushCommand(const Command & command)
	{
		m_commands.push_back(command);
	}

	//Get command
	template<typename COMMAND_TYPE>
	inline typename CommandBuffer<COMMAND_TYPE>::Command CommandBuffer<COMMAND_TYPE>::GetCommand(size_t & offset)
	{
		return m_commands[offset++];
	}

	//Returns the offset needed for alignment for ptr
	template<typename COMMAND_TYPE>
	inline size_t CommandBuffer<COMMAND_TYPE>::CalculateAlignment(size_t alignment, size_t offset)
	{
		size_t bias = offset % alignment;
		return (bias == 0) ? 0 : (alignment - bias);
	}

	//Push command data
	template<typename COMMAND_TYPE>
	template<typename DATA>
	inline void CommandBuffer<COMMAND_TYPE>::PushData(const DATA & data)
	{
		size_t alignment_offset = CalculateAlignment(alignof(DATA), m_command_data.size());

		//Reserve memory as needed
		const size_t begin_offset = m_command_data.size() + alignment_offset;
		m_command_data.resize(alignment_offset + sizeof(DATA));

		//Copy data
		memcpy(&m_command_data[begin_offset], &data, sizeof(DATA));
	}

	//Push command data array
	template<typename COMMAND_TYPE>
	template<typename DATA>
	inline void CommandBuffer<COMMAND_TYPE>::PushDataArray(const DATA * data, size_t num)
	{
		size_t alignment_offset = CalculateAlignment(alignof(DATA), m_command_data.size());

		//Reserve memory as needed
		const size_t begin_offset = m_command_data.size() + alignment_offset;
		m_command_data.resize(alignment_offset + sizeof(DATA) * num);

		//Copy data
		memcpy(&m_command_data[begin_offset], data, sizeof(DATA) * num);
	}

	//Get command data
	template<typename COMMAND_TYPE>
	template<typename DATA>
	inline DATA & CommandBuffer<COMMAND_TYPE>::GetData(size_t & offset)
	{
		size_t alignment_offset = CalculateAlignment(alignof(DATA), m_command_data.size());
		const size_t begin_offset = m_command_data.size() + alignment_offset;

		//Move offset
		offset += alignment_offset + sizeof(DATA);
		//Return data
		return *reinterpret_cast<DATA*>(&m_command_data[begin_offset]);
	}

	//Get command data array
	template<typename COMMAND_TYPE>
	template<typename DATA>
	inline DATA * CommandBuffer<COMMAND_TYPE>::GetDataArray(size_t & offset, size_t num)
	{
		size_t alignment_offset = CalculateAlignment(alignof(DATA), m_command_data.size());
		const size_t begin_offset = m_command_data.size() + alignment_offset;

		//Move offset
		offset += alignment_offset + sizeof(DATA) * num;
		//Return data
		return reinterpret_cast<DATA*>(&m_command_data[begin_offset]);
	}
}

#endif //COMMAND_BUFFER_h
