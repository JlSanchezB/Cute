#include <core/log.h>
#include <windows.h>

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
}

namespace core
{
	void log_info(const char* message, ...)
	{
		char buffer[kLogFormatBufferSize];
		va_list args;
		va_start(args, message);
		int used =  ImFormatStringV(buffer, kLogFormatBufferSize, message, args);
		OutputDebugString("INFO: ");
		OutputDebugString(buffer);
		OutputDebugString("\n");
		va_end(args);
	}

	void log_warning(const char* message, ...)
	{
		char buffer[kLogFormatBufferSize];
		va_list args;
		va_start(args, message);
		int used = ImFormatStringV(buffer, kLogFormatBufferSize, message, args);
		OutputDebugString("WARNING: ");
		OutputDebugString(buffer);
		OutputDebugString("\n");
		va_end(args);
	}

	void log_error(const char* message, ...)
	{
		char buffer[kLogFormatBufferSize];
		va_list args;
		va_start(args, message);
		int used = ImFormatStringV(buffer, kLogFormatBufferSize, message, args);
		OutputDebugString("ERROR: ");
		OutputDebugString(buffer);
		OutputDebugString("\n");
		va_end(args);
	}
}