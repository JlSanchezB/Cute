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
}

namespace core
{
	void log(const char* message, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, message);
		int used =  ImFormatStringV(buffer, 1024, message, args);
		OutputDebugString(buffer);
		va_end(args);
	}

	void log_warning(const char* message, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, message);
		int used = ImFormatStringV(buffer, 1024, message, args);
		OutputDebugString(buffer);
		va_end(args);
	}

	void log_error(const char* message, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, message);
		int used = ImFormatStringV(buffer, 1024, message, args);
		OutputDebugString(buffer);
		va_end(args);
	}
}