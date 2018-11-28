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

	constexpr size_t kLogBufferSize = 1024;
}

namespace core
{
	void log_info(const char* message, ...)
	{
		char buffer[kLogBufferSize];
		va_list args;
		va_start(args, message);
		int used =  ImFormatStringV(buffer, kLogBufferSize, message, args);
		OutputDebugString("INFO: ");
		OutputDebugString(buffer);
		OutputDebugString("\n");
		va_end(args);
	}

	void log_warning(const char* message, ...)
	{
		char buffer[kLogBufferSize];
		va_list args;
		va_start(args, message);
		int used = ImFormatStringV(buffer, kLogBufferSize, message, args);
		OutputDebugString("WARNING: ");
		OutputDebugString(buffer);
		OutputDebugString("\n");
		va_end(args);
	}

	void log_error(const char* message, ...)
	{
		char buffer[kLogBufferSize];
		va_list args;
		va_start(args, message);
		int used = ImFormatStringV(buffer, kLogBufferSize, message, args);
		OutputDebugString("ERROR: ");
		OutputDebugString(buffer);
		OutputDebugString("\n");
		va_end(args);
	}
}