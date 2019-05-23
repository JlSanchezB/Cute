#include <core/log.h>
#include <core/sync.h>
#include <windows.h>
#include <array>
#include <ext/imgui/imgui.h>

namespace
{
	int ImFormatStringV(char* buf, size_t buf_size, const char* fmt, va_list args)
	{
		int w = vsnprintf_s(buf, buf_size, buf_size - 1 , fmt, args);
		if (buf == NULL)
			return w;
		if (w == -1 || w >= (int)buf_size)
			w = (int)buf_size - 1;
		buf[w] = 0;
		return w;
	}

	//Size used for the format intermediated buffer
	constexpr size_t kLogFormatBufferSize = 1024;

	enum class Priority : uint8_t
	{
		Error,
		Warning,
		Info,
		Free //Used to detect free spaces in the log buffer
	};

	struct LogSlot
	{
		Priority priority;
		uint32_t size; //Reserved buffer size, multipler of sizeof(LogSlot)
	};

	//Log ring buffer size
	constexpr size_t kLogBufferSize = 12 * 1024;

	constexpr ::std::array<char, kLogBufferSize> InitLogBuffer()
	{
		std::array<char, kLogBufferSize> buffer = {};
		LogSlot* log_slot = (LogSlot*)(buffer.data());
		log_slot->priority = Priority::Free;
		log_slot->size = static_cast<uint32_t>(kLogBufferSize - sizeof(LogSlot));

		return buffer;
	}

	//Log buffer
	alignas(alignof(LogSlot)) std::array<char, kLogBufferSize> g_log_buffer = InitLogBuffer();

	//Top slot in the log buffer
	size_t g_top_log_slot = 0;

	//Access mutex
	core::SpinLockMutex g_log_mutex;

	//Allocate slot, returns a free const char buffer ready to copy the log
	void AllocSlot(Priority priority, size_t size, char*& message_slot_buffer)
	{
		core::SpinLockMutexGuard log_access_guard(g_log_mutex);

		//Align size with sizeof LogSlot
		size = (((size - 1) / alignof(LogSlot)) + 1) * alignof(LogSlot);

		//Get Top Slot
		LogSlot* top_slot = reinterpret_cast<LogSlot*>(&g_log_buffer[g_top_log_slot]);
		size_t free_space = top_slot->size;
		
		//Check if it has space or needs more to use more slots
		size_t next_slot = g_top_log_slot + sizeof(LogSlot) + top_slot->size;
		while (free_space < size)
		{
			//Check it the end of the buffer has been hit
			if (next_slot == kLogBufferSize)
			{
				//We need to reset all and start allocate from the head

				//First, leave a free slot at the end of the buffer
				top_slot->priority = Priority::Free;
				top_slot->size = static_cast<uint32_t>(free_space);

				//Start again with the first slot
				top_slot = reinterpret_cast<LogSlot*>(&g_log_buffer[0]);
				free_space = top_slot->size;
				g_top_log_slot = 0;
				next_slot = g_top_log_slot + sizeof(LogSlot) + top_slot->size;
				continue;
			}

			//Grow with the next slot
			LogSlot* next_slot_data = reinterpret_cast<LogSlot*>(&g_log_buffer[next_slot]);
			next_slot += sizeof(LogSlot) + next_slot_data->size;
			free_space += sizeof(LogSlot) + next_slot_data->size;
		}

		top_slot->priority = priority;

		//Check if there still free space to create another slot
		if ((free_space - size) > (sizeof(LogSlot) + 20))
		{
			top_slot->size = static_cast<uint32_t>(size);
			//Return the message buffer
			message_slot_buffer = &g_log_buffer[g_top_log_slot + sizeof(LogSlot)];
			g_top_log_slot += sizeof(LogSlot) + size;

			//Register the left like free
			LogSlot* free_slot = reinterpret_cast<LogSlot*>(&g_log_buffer[g_top_log_slot]);
			free_slot->priority = Priority::Free;
			free_slot->size = static_cast<uint32_t>(free_space - size - sizeof(LogSlot));
		}
		else
		{
			top_slot->size = static_cast<uint32_t>(free_space);
			//Return the message buffer
			message_slot_buffer = &g_log_buffer[g_top_log_slot + sizeof(LogSlot)];
			g_top_log_slot += sizeof(LogSlot) + free_space;
		}

		//Check if it is end of the buffer
		if (g_top_log_slot == kLogBufferSize)
		{
			g_top_log_slot = 0;
		}
	}

	void Log(Priority priority, const char* message, va_list args)
	{
		char buffer[kLogFormatBufferSize];

		size_t used = ImFormatStringV(buffer, kLogFormatBufferSize, message, args);

		//Add to the log
		char* dest_buffer = nullptr;
		AllocSlot(priority, used + 1, dest_buffer);
		memcpy(dest_buffer, buffer, used + 1);

		switch (priority)
		{
		case Priority::Info:
			OutputDebugString("INFO: ");
			break;
		case Priority::Warning:
			OutputDebugString("WARNING: ");
			break;
		case Priority::Error:
			OutputDebugString("ERROR: ");
			break;
		default:
			break;
		}
		OutputDebugString(buffer);
		OutputDebugString("\n");
	}
}

namespace core
{
	void LogInfo(const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		Log(Priority::Info, message, args);
		va_end(args);
	}

	void LogWarning(const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		Log(Priority::Warning, message, args);
		va_end(args);
	}

	void LogError(const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		Log(Priority::Error, message, args);
		va_end(args);
	}

	bool LogRender()
	{
		bool open = true;

		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Logger", &open))
		{
			ImGui::End();
			return open;
		}
		bool scroll_to_end = ImGui::Button("Scroll to end");
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		//Print all the log
		size_t current_log_slot_index = g_top_log_slot;
		LogSlot* current_log_slot = reinterpret_cast<LogSlot*>(&g_log_buffer[current_log_slot_index]);
		
		do
		{
			if (current_log_slot->priority != Priority::Free)
			{
				switch (current_log_slot->priority)
				{
				case Priority::Info:
					ImGui::Text(&g_log_buffer[current_log_slot_index] + sizeof(LogSlot));
					break;
				case Priority::Warning:
					ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.1f, 1.f), &g_log_buffer[current_log_slot_index] + sizeof(LogSlot));
					break;
				case Priority::Error:
					ImGui::TextColored(ImVec4(0.9f, 0.1f, 0.1f, 1.f), &g_log_buffer[current_log_slot_index] + sizeof(LogSlot));
					break;
				default:
					break;
				}
				
			}
			current_log_slot_index += sizeof(LogSlot) + current_log_slot->size;
			if (current_log_slot_index == kLogBufferSize)
			{
				//End of the ring buffer
				current_log_slot_index = 0;
			}
			current_log_slot = reinterpret_cast<LogSlot*>(&g_log_buffer[current_log_slot_index]);
		
		} while (g_top_log_slot != current_log_slot_index);

		if (scroll_to_end)
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
		ImGui::End();

		return open;
	}
}