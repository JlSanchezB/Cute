#include <core/log.h>

#include <windows.h>

namespace core
{
	void log(const char* message)
	{
		OutputDebugString(message);
	}
}