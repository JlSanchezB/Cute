#include <stdarg.h>

namespace
{
	inline void AddError(render::LoadContext& load_context, const char* message, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, message);
		vsnprintf_s(buffer, 1024, 1024 - 1, message, args);
		va_end(args);

		load_context.errors.push_back(buffer);
	}
}
