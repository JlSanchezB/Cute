//////////////////////////////////////////////////////////////////////////
// Cute engine - Virtual command buffer that captures render commands from the game to the render
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_COMMAND_BUFFER_h
#define RENDER_COMMAND_BUFFER_h

#include <display/display_desc.h>
#include <vector>

namespace render
{
	class CommandBuffer
	{
	public:
		using CommandOffset = uint32_t;

	private:
		using Command = uint8_t;
		//List of commands in the command buffer
		std::vector<Command> m_commands;
		//Data associated to each command
		std::vector<uint8_t> m_command_data;
	};
}

#endif //RENDER_COMMAND_BUFFER_h
